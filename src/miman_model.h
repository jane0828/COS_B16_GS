#pragma once
#include "miman_config.h"
using namespace std;

GLint TextureFromFile( const char *path, string directory );

void * SatelliteInitialize(void * );
void SatelliteModelInitialize(int index);
void SatelliteModelDelete(int index);
void SatDisplayer(GLint * SatTexture);
void PathInitializer();
void SatpointDisplayer();
void PathGenerator(int Selected);
void PathDisplayer(int Selected);
void PathDelete();

class Model 
{
public:

    vector<Mesh> meshes;
    vector<Texture> textures_loaded;    // Stores all the textures loaded so far,
    // optimization to make sure textures aren't
    // loaded more than once.
    
    /*  Functions   */
    // Constructor, expects a filepath to a 3D model.
    Model( GLchar *path )
    {
        this->loadModel( path );
    }
    
    // Draws the model, and thus all its meshes
    void Draw( Shader *shader )
    {
        for ( GLuint i = 0; i < this->meshes.size( ); i++ )
        {
            this->meshes[i].Draw( shader );
        }
    }
    
private:
    
    string directory;
    
    /*  Functions   */
    // Loads a model with supported ASSIMP extensions from file and stores the resulting meshes in the meshes vector.
    void loadModel( string path )
    {
        // Read file via ASSIMP
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile( path, aiProcess_Triangulate | aiProcess_FlipUVs );
        
        // Check for errors
        if( !scene || scene->mFlags == AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode ) // if is Not Zero
        {
            cout << "ERROR::ASSIMP:: " << importer.GetErrorString( ) << endl;
            return;
        }
        // Retrieve the directory path of the filepath
        this->directory = path.substr( 0, path.find_last_of( '/' ) );
        
        // Process ASSIMP's root node recursively
        this->processNode( scene->mRootNode, scene );
    }
    
    // Processes a node in a recursive fashion. Processes each individual mesh located at the node and repeats this process on its children nodes (if any).
    void processNode( aiNode* node, const aiScene* scene )
    {
        // Process each mesh located at the current node
        for ( GLuint i = 0; i < node->mNumMeshes; i++ )
        {
            // The node object only contains indices to index the actual objects in the scene.
            // The scene contains all the data, node is just to keep stuff organized (like relations between nodes).
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            
            this->meshes.push_back( this->processMesh( mesh, scene ) );
        }
        
        // After we've processed all of the meshes (if any) we then recursively process each of the children nodes
        for ( GLuint i = 0; i < node->mNumChildren; i++ )
        {
            this->processNode( node->mChildren[i], scene );
        }
    }
    
    Mesh processMesh( aiMesh *mesh, const aiScene *scene )
    {
        // Data to fill
        vector<Vertex> vertices;
        vector<GLuint> indices;
        vector<Texture> textures;
        Material color;
        
        // Walk through each of the mesh's vertices
        for ( GLuint i = 0; i < mesh->mNumVertices; i++ )
        {
            Vertex vertex;
            glm::vec3 vector; // We declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
            
            // Positions
            vector.x = mesh->mVertices[i].x;
            vector.y = mesh->mVertices[i].y;
            vector.z = mesh->mVertices[i].z;
            vertex.Position = vector;
            
            // Normals
            vector.x = mesh->mNormals[i].x;
            vector.y = mesh->mNormals[i].y;
            vector.z = mesh->mNormals[i].z;
            vertex.Normal = vector;
            
            // Texture Coordinates
            if( mesh->mTextureCoords[0] ) // Does the mesh contain texture coordinates?
            {
                glm::vec2 vec;
                // A vertex can contain up to 8 different texture coordinates. We thus make the assumption that we won't
                // use models where a vertex can have multiple texture coordinates so we always take the first set (0).
                vec.x = mesh->mTextureCoords[0][i].x;
                vec.y = mesh->mTextureCoords[0][i].y;
                vertex.TexCoords = vec;
            }
            else
            {
                vertex.TexCoords = glm::vec2( 0.0f, 0.0f );
            }
            
            vertices.push_back( vertex );
        }
        
        // Now wak through each of the mesh's faces (a face is a mesh its triangle) and retrieve the corresponding vertex indices.
        for ( GLuint i = 0; i < mesh->mNumFaces; i++ )
        {
            aiFace face = mesh->mFaces[i];
            // Retrieve all indices of the face and store them in the indices vector
            for ( GLuint j = 0; j < face.mNumIndices; j++ )
            {
                indices.push_back( face.mIndices[j] );
            }
        }

        // Process materials
        if( mesh->mMaterialIndex >= 0 )
        {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            // We assume a convention for sampler names in the shaders. Each diffuse texture should be named
            // as 'texture_diffuseN' where N is a sequential number ranging from 1 to MAX_SAMPLER_NUMBER.
            // Same applies to other texture as the following list summarizes:
            // Diffuse: texture_diffuseN
            // Specular: texture_specularN
            // Normal: texture_normalN
            
            // 1. Diffuse maps
            vector<Texture> diffuseMaps = this->loadMaterialTextures( material, aiTextureType_DIFFUSE, "texture_diffuse" );
            textures.insert( textures.end( ), diffuseMaps.begin( ), diffuseMaps.end( ) );
            
            // 2. Specular maps
            vector<Texture> specularMaps = this->loadMaterialTextures( material, aiTextureType_SPECULAR, "texture_specular" );
            textures.insert( textures.end( ), specularMaps.begin( ), specularMaps.end( ) );

            // Material only without texture
            color = this->loadMaterial( material );
        }
        
        // Return a mesh object created from the extracted mesh data
        return Mesh( vertices, indices, textures, color );
    }
    
    // Checks all material textures of a given type and loads the textures if they're not loaded yet.
    // The required info is returned as a Texture struct.
    vector<Texture> loadMaterialTextures( aiMaterial *mat, aiTextureType type, string typeName )
    {
        vector<Texture> textures;
        for ( GLuint i = 0; i < mat->GetTextureCount( type ); i++ )
        {
            aiString str;
            mat->GetTexture( type, i, &str );
            
            // Check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
            GLboolean skip = false;
            
            for ( GLuint j = 0; j < textures_loaded.size( ); j++ )
            {
                if( textures_loaded[j].path == str )
                {
                    textures.push_back( textures_loaded[j] );
                    skip = true; // A texture with the same filepath has already been loaded, continue to next one. (optimization)
                    
                    break;
                }
            }
            
            if( !skip )
            {   // If texture hasn't been loaded already, load it
                Texture texture;
                texture.id = TextureFromFile( str.C_Str( ), this->directory );
                texture.type = typeName;
                texture.path = str;
                textures.push_back( texture );
                
                this->textures_loaded.push_back( texture );  // Store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
            }
        }
        
        return textures;
    }

    Material loadMaterial(aiMaterial* mat) {
		Material material;
		aiColor3D color(0.f, 0.f, 0.f);
		float shininess;

		mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
		material.Diffuse = glm::vec3(color.r, color.g, color.b);

		mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
		material.Ambient = glm::vec3(color.r, color.g, color.b);

		mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
		material.Specular = glm::vec3(color.r, color.g, color.b);

		mat->Get(AI_MATKEY_SHININESS, shininess);
		material.Shininess = shininess;

		return material;
    }
};

