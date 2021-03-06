//
// Created by javan on 2/24/2018.
//
#include "assimpLoader.h"
#include "myShader.h"
#include "misc.h"
#include <opencv2/opencv.hpp>

AssimpLoader::AssimpLoader() {
    importerPtr = new Assimp::Importer;
    scene = NULL;
    isObjectLoaded = false;
    // shader related setup -- loading, attribute and uniform locations
    std::string vertexShader = "shaders/modelTextured.vsh";
    std::string fragmentShader = "shaders/modelTextured.fsh";
    shaderProgramID = LoadShaders(vertexShader, fragmentShader);
    vertexAttribute = GetAttributeLocation(shaderProgramID, "vertexPosition");
    vertexUVAttribute = GetAttributeLocation(shaderProgramID, "vertexUV");
    mvpLocation = GetUniformLocation(shaderProgramID, "mvpMat");
    textureSamplerLocation = GetUniformLocation(shaderProgramID, "textureSampler");

    /// not used jast sampels !//
//    attribute_v_coord = GetAttributeLocation(shaderProgramID, "v_coord");
//    attribute_v_normal = GetAttributeLocation(shaderProgramID, "v_normal");
//    uniform_m = GetUniformLocation(shaderProgramID, "m");
//    uniform_p = GetUniformLocation(shaderProgramID, "p");
//    uniform_v = GetUniformLocation(shaderProgramID, "v");
//    uniform_m_3x3_inv_transp = GetUniformLocation(shaderProgramID, "m_3x3_inv_transp");
//    uniform_v_inv = GetUniformLocation(shaderProgramID, "v_inv");

    CheckGLError("AssimpLoader::AssimpLoader");
}

/**
 * Class destructor, deletes Assimp importer pointer and removes 3D model from GL
 */
AssimpLoader::~AssimpLoader() {
    Delete3DModel();
    if (importerPtr) {
        delete importerPtr;
        importerPtr = NULL;
    }
    scene = NULL; // gets deleted along with importerPtr
}

/**
 * Generate buffers for vertex positions, texture coordinates, faces -- and load data into them
 */
void AssimpLoader::GenerateGLBuffers() {

    struct MeshInfo newMeshInfo; // this struct is updated for each mesh in the model
    GLuint buffer;

    // For every mesh -- load face indices, vertex positions, vertex texture coords
    // also copy texture index for mesh into newMeshInfo.textureIndex
    for (unsigned int n = 0; n < scene->mNumMeshes; ++n) {

        const aiMesh *mesh = scene->mMeshes[n]; // read the n-th mesh

        newMeshInfo.mesh = scene->mMeshes[n];
        // create array with faces
        // convert from Assimp's format to array for GLES
        unsigned short *faceArray = new unsigned short[sizeof(unsigned short) * mesh->mNumFaces *
                                                       3];
        unsigned int faceIndex = 0;
        for (unsigned int t = 0; t < mesh->mNumFaces; ++t) {

            // read a face from assimp's mesh and copy it into faceArray
            const aiFace *face = &mesh->mFaces[t];
            memcpy(&faceArray[faceIndex], face->mIndices, 3 * sizeof(unsigned short));
            faceIndex += 3;

        }

        newMeshInfo.numberOfFaces = scene->mMeshes[n]->mNumFaces;
        newMeshInfo.numberOfVertice = scene->mMeshes[n]->mNumVertices;

        // buffer for faces
        if (newMeshInfo.numberOfFaces) {

            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         sizeof(unsigned short) * mesh->mNumFaces * 3, faceArray,
                         GL_STATIC_DRAW);
            newMeshInfo.faceBuffer = buffer;

        }
        delete[] faceArray;

        // buffer for vertex positions
        if (mesh->HasPositions()) {

            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(float) * 3 * mesh->mNumVertices, mesh->mVertices,
                         GL_STATIC_DRAW);
            newMeshInfo.vertexBuffer = buffer;

        }

        // buffer for vertex texture coordinates
        // ***ASSUMPTION*** -- handle only one texture for each mesh
        if (mesh->HasTextureCoords(0)) {

            float *textureCoords = new float[2 * mesh->mNumVertices];
            for (unsigned int k = 0; k < mesh->mNumVertices; ++k) {
                textureCoords[k * 2] = mesh->mTextureCoords[0][k].x;
                textureCoords[k * 2 + 1] = mesh->mTextureCoords[0][k].y;
            }
            glGenBuffers(1, &buffer);
            glBindBuffer(GL_ARRAY_BUFFER, buffer);
            glBufferData(GL_ARRAY_BUFFER,
                         sizeof(float) * 2 * mesh->mNumVertices, textureCoords,
                         GL_STATIC_DRAW);
            newMeshInfo.textureCoordBuffer = buffer;
            delete[] textureCoords;

        }

        // unbind buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        // copy texture index (= texture name in GL) for the mesh from textureNameMap
        aiMaterial *mtl = scene->mMaterials[mesh->mMaterialIndex];
        aiString texturePath;    //contains filename of texture
        if (AI_SUCCESS == mtl->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath)) {
            unsigned short textureId = textureNameMap[texturePath.data];
            newMeshInfo.textureIndex = textureId;
        } else {
            newMeshInfo.textureIndex = 0;
        }

        modelMeshes.push_back(newMeshInfo);
    }
}

/**
 * Read textures associated with all materials and load images to GL
 */
bool AssimpLoader::LoadTexturesToGL(std::string modelFilename) {

}

/**
 * Loads a general OBJ with many meshes -- assumes texture is associated with each mesh
 * does not handle material properties (like diffuse, specular, etc.)
 */
bool AssimpLoader::Load3DModel(std::string modelFilename) {

    MyLOGI("Scene will be imported now");
    scene = importerPtr->ReadFile(modelFilename, aiProcessPreset_TargetRealtime_Quality);

    // Check if import failed
    if (!scene) {
        std::string errorString = importerPtr->GetErrorString();
        MyLOGE("Scene import failed: %s", errorString.c_str());
        return false;
    }
    MyLOGI("Imported %s successfully.", modelFilename.c_str());

    if (!LoadTexturesToGL(modelFilename)) {
        MyLOGE("Unable to load textures");
        return false;
    }
    MyLOGI("Loaded textures successfully");

    GenerateGLBuffers();
    MyLOGI("Loaded vertices and texture coords successfully");

    isObjectLoaded = true;
    return true;
}

void AssimpLoader::MyLoad3DModel(MyMesh *mesh, glm::mat4 *mvp) {
    glEnable(GL_CULL_FACE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glUseProgram(shaderProgramID);
    //mesh->draw(attribute_v_coord, attribute_v_normal, uniform_m, uniform_m_3x3_inv_transp, mvp);
}

/**
 * Clears memory associated with the 3D model
 */
void AssimpLoader::Delete3DModel() {
    /*if (isObjectLoaded) {
        // clear modelMeshes stuff
        for (unsigned int i = 0; i < modelMeshes.size(); ++i) {
            // glDeleteTextures(1, &(modelMeshes[i].textureIndex));
        }
        modelMeshes.clear();

        MyLOGI("Deleted Assimp object");
        isObjectLoaded = false;
    }*/
}

/**
 * Renders the 3D model by rendering every mesh in the object
 */
void AssimpLoader::Render3DModel(glm::mat4 *mvpMat) {

    if (!isObjectLoaded) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shaderProgramID);
    glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, (const GLfloat *) mvpMat);

    //glActiveTexture(GL_TEXTURE0);
    glUniform1i(textureSamplerLocation, 0);

    unsigned int numberOfLoadedMeshes = modelMeshes.size();

    // render all meshes

    // render all meshes
    for (unsigned int n = 0; n < numberOfLoadedMeshes; ++n) {
        // Texture
        if (modelMeshes[n].textureIndex) {
            glBindTexture(GL_TEXTURE_2D, modelMeshes[n].textureIndex);
        }
        // Faces
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, modelMeshes[n].faceBuffer);


        // Vertices
        glBindBuffer(GL_ARRAY_BUFFER, modelMeshes[n].vertexBuffer);
        glEnableVertexAttribArray(vertexAttribute);
        glVertexAttribPointer(vertexAttribute, 3, GL_FLOAT, 0, 0, 0);


        /* Use the index data loaded from the Open Asset Importer. */
        //glDrawElements(GL_TRIANGLES, scene->mMeshes[n]->mNumFaces, GL_UNSIGNED_BYTE, &indices[n]);

        // Texture coords
        // glBindBuffer(GL_ARRAY_BUFFER, modelMeshes[n].textureCoordBuffer);
        //glEnableVertexAttribArray(vertexUVAttribute);
        //glVertexAttribPointer(vertexUVAttribute, 2, GL_FLOAT, 0, 0, 0);

        int x = modelMeshes[n].numberOfFaces*3;

        /* if (x == 6)
             glDrawArrays(GL_TRIANGLE_FAN, 0, x);
         else glDrawArrays(GL_TRIANGLE_FAN, 0, 6);*/

        MyLOGE("numberOfFaces %d", x);
        MyLOGE("textureIndex %d", modelMeshes[n].textureIndex);

        //for (int i = 0; i < x / 3; ++i)
        if (x > 3) {
            for (int i = 0; i < x / 6 - 2; ++i)
                glDrawArrays(GL_TRIANGLE_FAN, 0, 3);
        } else
            glDrawArrays(GL_TRIANGLE_FAN, modelMeshes[n].textureIndex, x);
        // glDrawArrays(GL_TRIANGLE_FAN, i, 3);
        // glDrawElements(GL_TRIANGLES, x, GL_UNSIGNED_INT, 0);//GL_LINE_LOOP

        // unbind buffers
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glDisableVertexAttribArray(vertexAttribute);
        glDisableVertexAttribArray(vertexUVAttribute);


    }
    // MyLOGE("==================================================");
    CheckGLError("AssimpLoader::renderObject() ");
}

void AssimpLoader::Render3DModelnew(unsigned i) {
    int vertices_accumulation = 0;

    //for (int i = 0; i < scene->mNumMeshes; i++)
//    {
//        for (int j = 0; j < scene->mMeshes[i]->mNumVertices; j++) {
//            const aiVector3D vector3D = scene->mMeshes[i]->mVertices[j];
//            vertex.push_back(vector3D.x);
//            vertex.push_back(vector3D.y);
//            vertex.push_back(vector3D.z);
//        }
//        for (unsigned int j = 0; j < scene->mMeshes[i]->mNumFaces; j++) {
//            const aiFace aiFace1 = scene->mMeshes[i]->mFaces[j];
//            // for (unsigned int *i = 0; i < aiFace1.mIndices; ++i)
//            {
//                indices.push_back(aiFace1.mIndices[0] + vertices_accumulation);
//                indices.push_back(aiFace1.mIndices[1] + vertices_accumulation);
//                indices.push_back(aiFace1.mIndices[2] + vertices_accumulation);
//            }
//        }
//        vertices_accumulation += scene->mMeshes[i]->mNumVertices;
//    }
}