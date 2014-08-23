#include "stdafx.h"
#include "PhysicsBlocks.h"

#include "Actor.h"
#include "BlockData.h"
#include "Chunk.h"
#include "ChunkUpdater.h"
#include "Frustum.h"
#include "GameManager.h"
#include "OpenglManager.h"
#include "Options.h"
#include "Particles.h"
#include "PhysicsEngine.h"
#include "TerrainGenerator.h"
#include "Texture2d.h"
#include "shader.h"
#include "utils.h"

PhysicsBlock::PhysicsBlock(const glm::dvec3 &pos, int BlockType, int ydiff, glm::vec2 &dir, glm::vec3 extraForce)
{
    int btype;
    double v = 0.0;
    bool tree = 0;
    done = 0;
    if (dir[0] != 0 || dir[1] != 0) tree = 1;

    blockType = BlockType;
    btype = (GETBLOCKTYPE(BlockType));
    position = pos;
    int flags = GETFLAGS(BlockType) >> 12;
    if (ydiff < 0) ydiff = -ydiff;

    grav = GRAVITY;
    fric = 0.985f;
    if (ydiff > 50){
        if (tree){
            grav = GRAVITY;
            fric = 0.98f - 0.02f;
        }
        v = 1.0;
    } else if (ydiff > 1){
        v = (ydiff - 1) / 49.0f;
        if (tree){
            grav = GRAVITY;
            fric = 0.98f - 0.02*(ydiff - 1) / 49.0f;
        }
    }

    if (v){
        velocity.x = ((rand() % 100) * .001 - 0.05 + dir[0] * 1.65f)*v;
        velocity.y = 0;
        velocity.z = ((rand() % 100) * .001 - 0.05 + dir[1] * 1.65f)*v;
    } else{
        velocity = glm::vec3(0.0f);
    }

    velocity += extraForce;

    light[0] = 0;
    light[1] = 255;

}

int bdirs[96] = { 0, 1, 2, 3, 0, 1, 3, 2, 0, 2, 3, 1, 0, 2, 1, 3, 0, 3, 2, 1, 0, 3, 1, 2,
1, 0, 2, 3, 1, 0, 3, 2, 1, 2, 0, 3, 1, 2, 3, 0, 1, 3, 2, 0, 1, 3, 0, 2,
2, 0, 1, 3, 2, 0, 3, 1, 2, 1, 3, 0, 2, 1, 0, 3, 2, 3, 0, 1, 2, 3, 1, 0,
3, 0, 1, 2, 3, 0, 2, 1, 3, 1, 2, 0, 3, 1, 0, 2, 3, 2, 0, 1, 3, 2, 1, 0 };

const GLushort boxIndices[36] = { 0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 8, 9, 10, 10, 11, 8, 12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20 };

const GLfloat physicsBlockVertices[72] = { -0.499f, 0.499f, 0.499f, -0.499f, -0.499f, 0.499f, 0.499f, -0.499f, 0.499f, 0.499f, 0.499f, 0.499f,  // v1-v2-v3-v0 (front)

0.499f, 0.499f, 0.499f, 0.499f, -0.499f, 0.499f, 0.499f, -0.499f, -0.499f, 0.499f, 0.499f, -0.499f,     // v0-v3-v4-v499 (right)

-0.499f, 0.499f, -0.499f, -0.499f, 0.499f, 0.499f, 0.499f, 0.499f, 0.499f, 0.499f, 0.499f, -0.499f,    // v6-v1-v0-v499 (top)

-0.499f, 0.499f, -0.499f, -0.499f, -0.499f, -0.499f, -0.499f, -0.499f, 0.499f, -0.499f, 0.499f, 0.499f,   // v6-v7-v2-v1 (left)

-0.499f, -0.499f, -0.499f, 0.499f, -0.499f, -0.499f, 0.499f, -0.499f, 0.499f, -0.499f, -0.499f, 0.499f,    // v7-v4-v3-v2 (bottom)

0.499f, 0.499f, -0.499f, 0.499f, -0.499f, -0.499f, -0.499f, -0.499f, -0.499f, -0.499f, 0.499f, -0.499f };     // v5-v4-v7-v6 (back)

inline void MakeFace(vector <PhysicsBlockVertex> &verts, const GLfloat *blockPositions, int vertexOffset, int &index, const BlockTexture& blockTexture)
{
    ui8 textureAtlas = (ui8)(blockTexture.base.textureIndex / 256);
    ui8 textureIndex = (ui8)(blockTexture.base.textureIndex % 256);

    ui8 overlayTextureAtlas = (ui8)(blockTexture.overlay.textureIndex / 256);
    ui8 overlayTextureIndex = (ui8)(blockTexture.overlay.textureIndex % 256);

    ui8 blendMode = 0x25; //0x25 = 00 10 01 01
    switch (blockTexture.blendMode) {
    case BlendType::BLEND_TYPE_REPLACE:
        blendMode++; //Sets blendType to 00 01 01 10
        break;
    case BlendType::BLEND_TYPE_ADD:
        blendMode += 4; //Sets blendType to 00 01 10 01
        break;
    case BlendType::BLEND_TYPE_SUBTRACT:
        blendMode -= 4; //Sets blendType to 00 01 00 01
        break;
    case BlendType::BLEND_TYPE_MULTIPLY:
        blendMode -= 16; //Sets blendType to 00 01 01 01
        break;
    }
    verts[index].blendMode = blendMode;
    verts[index + 1].blendMode = blendMode;
    verts[index + 2].blendMode = blendMode;
    verts[index + 3].blendMode = blendMode;
    verts[index + 4].blendMode = blendMode;
    verts[index + 5].blendMode = blendMode;

    verts[index].textureWidth = (ui8)blockTexture.base.size.x;
    verts[index].textureHeight = (ui8)blockTexture.base.size.y;
    verts[index + 1].textureWidth = (ui8)blockTexture.base.size.x;
    verts[index + 1].textureHeight = (ui8)blockTexture.base.size.y;
    verts[index + 2].textureWidth = (ui8)blockTexture.base.size.x;
    verts[index + 2].textureHeight = (ui8)blockTexture.base.size.y;
    verts[index + 3].textureWidth = (ui8)blockTexture.base.size.x;
    verts[index + 3].textureHeight = (ui8)blockTexture.base.size.y;
    verts[index + 4].textureWidth = (ui8)blockTexture.base.size.x;
    verts[index + 4].textureHeight = (ui8)blockTexture.base.size.y;
    verts[index + 5].textureWidth = (ui8)blockTexture.base.size.x;
    verts[index + 5].textureHeight = (ui8)blockTexture.base.size.y;

    verts[index].overlayTextureWidth = (ui8)blockTexture.overlay.size.x;
    verts[index].overlayTextureHeight = (ui8)blockTexture.overlay.size.y;
    verts[index + 1].overlayTextureWidth = (ui8)blockTexture.overlay.size.x;
    verts[index + 1].overlayTextureHeight = (ui8)blockTexture.overlay.size.y;
    verts[index + 2].overlayTextureWidth = (ui8)blockTexture.overlay.size.x;
    verts[index + 2].overlayTextureHeight = (ui8)blockTexture.overlay.size.y;
    verts[index + 3].overlayTextureWidth = (ui8)blockTexture.overlay.size.x;
    verts[index + 3].overlayTextureHeight = (ui8)blockTexture.overlay.size.y;
    verts[index + 4].overlayTextureWidth = (ui8)blockTexture.overlay.size.x;
    verts[index + 4].overlayTextureHeight = (ui8)blockTexture.overlay.size.y;
    verts[index + 5].overlayTextureWidth = (ui8)blockTexture.overlay.size.x;
    verts[index + 5].overlayTextureHeight = (ui8)blockTexture.overlay.size.y;

    verts[index].normal[0] = cubeNormals[vertexOffset];
    verts[index].normal[1] = cubeNormals[vertexOffset + 1];
    verts[index].normal[2] = cubeNormals[vertexOffset + 2];
    verts[index + 1].normal[0] = cubeNormals[vertexOffset + 3];
    verts[index + 1].normal[1] = cubeNormals[vertexOffset + 4];
    verts[index + 1].normal[2] = cubeNormals[vertexOffset + 5];
    verts[index + 2].normal[0] = cubeNormals[vertexOffset + 6];
    verts[index + 2].normal[1] = cubeNormals[vertexOffset + 7];
    verts[index + 2].normal[2] = cubeNormals[vertexOffset + 8];
    verts[index + 3].normal[0] = cubeNormals[vertexOffset + 6];
    verts[index + 3].normal[1] = cubeNormals[vertexOffset + 7];
    verts[index + 3].normal[2] = cubeNormals[vertexOffset + 8];
    verts[index + 4].normal[0] = cubeNormals[vertexOffset + 9];
    verts[index + 4].normal[1] = cubeNormals[vertexOffset + 10];
    verts[index + 4].normal[2] = cubeNormals[vertexOffset + 11];
    verts[index + 5].normal[0] = cubeNormals[vertexOffset];
    verts[index + 5].normal[1] = cubeNormals[vertexOffset + 1];
    verts[index + 5].normal[2] = cubeNormals[vertexOffset + 2];

    verts[index].position[0] = cubeVertices[vertexOffset];
    verts[index].position[1] = cubeVertices[vertexOffset + 1];
    verts[index].position[2] = cubeVertices[vertexOffset + 2];
    verts[index + 1].position[0] = cubeVertices[vertexOffset + 3];
    verts[index + 1].position[1] = cubeVertices[vertexOffset + 4];
    verts[index + 1].position[2] = cubeVertices[vertexOffset + 5];
    verts[index + 2].position[0] = cubeVertices[vertexOffset + 6];
    verts[index + 2].position[1] = cubeVertices[vertexOffset + 7];
    verts[index + 2].position[2] = cubeVertices[vertexOffset + 8];
    verts[index + 3].position[0] = cubeVertices[vertexOffset + 6];
    verts[index + 3].position[1] = cubeVertices[vertexOffset + 7];
    verts[index + 3].position[2] = cubeVertices[vertexOffset + 8];
    verts[index + 4].position[0] = cubeVertices[vertexOffset + 9];
    verts[index + 4].position[1] = cubeVertices[vertexOffset + 10];
    verts[index + 4].position[2] = cubeVertices[vertexOffset + 11];
    verts[index + 5].position[0] = cubeVertices[vertexOffset];
    verts[index + 5].position[1] = cubeVertices[vertexOffset + 1];
    verts[index + 5].position[2] = cubeVertices[vertexOffset + 2];

    verts[index].tex[0] = 128;
    verts[index].tex[1] = 129;
    verts[index + 1].tex[0] = 128;
    verts[index + 1].tex[1] = 128;
    verts[index + 2].tex[0] = 129;
    verts[index + 2].tex[1] = 128;
    verts[index + 3].tex[0] = 129;
    verts[index + 3].tex[1] = 128;
    verts[index + 4].tex[0] = 129;
    verts[index + 4].tex[1] = 129;
    verts[index + 5].tex[0] = 128;
    verts[index + 5].tex[1] = 129;

    verts[index].textureAtlas = textureAtlas;
    verts[index + 1].textureAtlas = textureAtlas;
    verts[index + 2].textureAtlas = textureAtlas;
    verts[index + 3].textureAtlas = textureAtlas;
    verts[index + 4].textureAtlas = textureAtlas;
    verts[index + 5].textureAtlas = textureAtlas;

    verts[index].textureIndex = textureIndex;
    verts[index + 1].textureIndex = textureIndex;
    verts[index + 2].textureIndex = textureIndex;
    verts[index + 3].textureIndex = textureIndex;
    verts[index + 4].textureIndex = textureIndex;
    verts[index + 5].textureIndex = textureIndex;

    verts[index].overlayTextureAtlas = overlayTextureAtlas;
    verts[index + 1].overlayTextureAtlas = overlayTextureAtlas;
    verts[index + 2].overlayTextureAtlas = overlayTextureAtlas;
    verts[index + 3].overlayTextureAtlas = overlayTextureAtlas;
    verts[index + 4].overlayTextureAtlas = overlayTextureAtlas;
    verts[index + 5].overlayTextureAtlas = overlayTextureAtlas;

    verts[index].overlayTextureIndex = overlayTextureIndex;
    verts[index + 1].overlayTextureIndex = overlayTextureIndex;
    verts[index + 2].overlayTextureIndex = overlayTextureIndex;
    verts[index + 3].overlayTextureIndex = overlayTextureIndex;
    verts[index + 4].overlayTextureIndex = overlayTextureIndex;
    verts[index + 5].overlayTextureIndex = overlayTextureIndex;

}

bool PhysicsBlock::update(const deque < deque < deque < ChunkSlot* > > > &chunkList, double X, double Y, double Z)
{
    if (done == 2) return 1; //defer destruction by two frames for good measure
    if (done != 0) {
        done++;
        return 0;
    }

    int x, y, z, val;
    int gridRelY, gridRelX, gridRelZ;
    int bx, by, bz, btype;
    int c;
    int r;
    int *dr;
    int blockID = GETBLOCKTYPE(blockType);
    bool fc = 1, bc = 1, lc = 1, rc = 1;
    bool iscrush = 0;
    bool moved;
    Chunk *ch;

    if (globalDebug2 == 0) return 0;

    position += velocity * physSpeedFactor;
    velocity.y -= grav * physSpeedFactor;
    if (velocity.y < -5.3f) velocity.y = -5.3f;
    velocity.z *= fric;
    velocity.x *= fric;

    gridRelX = (position.x - X);
    gridRelY = (position.y - Y);
    gridRelZ = (position.z - Z);

    x = (gridRelX / CHUNK_WIDTH);
    y = (gridRelY / CHUNK_WIDTH);
    z = (gridRelZ / CHUNK_WIDTH);

    if (x < 0 || y < 0 || z < 0 || x >= csGridWidth || y >= csGridWidth || z >= csGridWidth){
        velocity = glm::vec3(0.0f);
        return 1;
    }
    ch = chunkList[y][z][x]->chunk;
    if ((!ch) || ch->isAccessible == 0) return 1;

    bx = gridRelX % CHUNK_WIDTH;
    by = gridRelY % CHUNK_WIDTH;
    bz = gridRelZ % CHUNK_WIDTH;

    c = bx + by*CHUNK_LAYER + bz*CHUNK_WIDTH;
    val = ch->getBlockID(c);
    if (Blocks[val].collide || (Blocks[val].physicsProperty == P_LIQUID && GETBLOCK(blockType).physicsProperty >= P_POWDER)){
        if (Blocks[btype = (blockID)].isCrushable){
            glm::vec4 color;
            color.r = Blocks[btype].color[0];
            color.g = Blocks[btype].color[1];
            color.b = Blocks[btype].color[2];
            color.a = 255;

            if (Blocks[btype].altColors.size()){
                GLuint flags = (blockID) >> 12;
                if (flags){
                    color.r = Blocks[btype].altColors[flags - 1].r;
                    color.g = Blocks[btype].altColors[flags - 1].g;
                    color.b = Blocks[btype].altColors[flags - 1].b;
                }
            }

            particleEngine.addParticles(BPARTICLES, glm::dvec3((int)position.x - 1.0, (int)position.y + 1.0, (int)position.z - 1.0), 0, 0.1, 300, 1, color, Blocks[GETBLOCKTYPE(blockType)].pxTex, 2.0f, 4);
            return 1;
        }
        double fx, fy, fz;
        if (position.x > 0){
            fx = position.x - ((int)(position.x)) - 0.5;
        } else{
            fx = position.x - ((int)(position.x)) + 0.5;
        }
        if (position.y > 0){
            fy = position.y - ((int)(position.y)) - 0.5;
        } else{
            fy = position.y - ((int)(position.y)) + 0.5;
        }
        if (position.z > 0){
            fz = position.z - ((int)(position.z)) - 0.5;
        } else{
            fz = position.z - ((int)(position.z)) + 0.5;
        }

        double Afx = ABS(fx);
        double Afy = ABS(fy);
        double Afz = ABS(fz);

        if (((fx > 0 && ((rc = Blocks[GETBLOCKTYPE(ch->getRightBlockData(c))].collide) == 0)) || (fx <= 0 && ((lc = Blocks[GETBLOCKTYPE(ch->getLeftBlockData(c))].collide) == 0))) && Afx >= Afy && Afx >= Afz){
            if (fx > 0){
                position.x += 0.5001 - Afx;
            } else{
                position.x -= 0.5001 - Afx;
            }
        } else if (((fz > 0 && (fc = Blocks[GETBLOCKTYPE(ch->getFrontBlockData(c))].collide) == 0) || (fz <= 0 && (bc = Blocks[GETBLOCKTYPE(ch->getBackBlockData(c))].collide) == 0)) && Afz > Afy){
            if (fz > 0){
                position.z += 0.5001 - Afz;
            } else{
                position.z -= 0.5001 - Afz;
            }
        } else{
            if (Blocks[val].isCrushable){
                ChunkUpdater::removeBlock(ch, c, 1);
            } else if (Blocks[GETBLOCKTYPE(ch->getTopBlockData(c))].collide == 0){
                if (by < CHUNK_WIDTH - 1){
                    if (Blocks[btype].explosivePower){
                        GameManager::physicsEngine->addExplosion(ExplosionNode(position, blockType));
                    } else{
                        ChunkUpdater::placeBlock(ch, c + CHUNK_LAYER, blockType);
                    }
                } else if (ch->top && ch->top->isAccessible){
                    if (Blocks[btype].explosivePower){
                        GameManager::physicsEngine->addExplosion(ExplosionNode(position, blockType));
                    } else{
                        ChunkUpdater::placeBlock(ch->top, c - CHUNK_SIZE + CHUNK_LAYER, blockType);
                    }
                }
                done = 1;
                //pop it into place
                position.x -= fx;
                position.y -= (fy - 0.5);
                position.z -= fz;
                return 0;
            } else{
                r = rand() % 24;
                dr = &(bdirs[r * 4]);
                moved = 0;
                for (int k = 0; k < 4; k++){
                    r = dr[k];
                    switch (r){
                    case 0:
                        if (fc == 0){
                            position.z += 0.5001 - Afz;
                            k = 4;
                            moved = 1;
                        }
                        break;
                    case 1:
                        if (bc == 0){
                            position.z -= 0.5001 - Afz;
                            k = 4;
                            moved = 1;
                        }
                        break;
                    case 2:
                        if (lc == 0){
                            position.x -= 0.5001 - Afx;
                            k = 4;
                            moved = 1;
                        }
                        break;
                    case 3:
                        if (rc == 0){
                            position.x += 0.5001 - Afx;
                            k = 4;
                            moved = 1;
                        }
                        break;
                    }
                }
                if (!moved){
                    velocity = glm::vec3(0);
                    position.y += 1.0 - (fy - 0.5);
                }
            }
            if (velocity.y > 0){
                velocity.y = -(velocity.y*0.6);
            }
            velocity.x *= 0.8;
            velocity.z *= 0.8;
        }
    } else if (Blocks[val].isCrushable){
        ChunkUpdater::removeBlock(ch, c, 1);
        return 0;
    } else{
        light[LIGHT] = (GLubyte)(255.0f*(LIGHT_OFFSET + pow(LIGHT_MULT, MAXLIGHT - ch->getLight(LIGHT, c))));
        light[SUNLIGHT] = (GLubyte)(255.0f*(LIGHT_OFFSET + pow(LIGHT_MULT, MAXLIGHT - ch->getLight(SUNLIGHT, c))));
    }
    return 0;
}

//temp and rain for dirtgrass
PhysicsBlockBatch::PhysicsBlockBatch(int BlockType, GLubyte temp, GLubyte rain) : blockType(BlockType), _mesh(NULL), _numBlocks(0)
{
    physicsBlocks.reserve(512);

    PhysicsBlockMeshMessage *pbmm = new PhysicsBlockMeshMessage;
    vector <PhysicsBlockVertex> &verts = pbmm->verts;
    verts.resize(36);

    int btype;
    double v = 0.0;
    bool tree = 0;

    btype = (GETBLOCKTYPE(BlockType));

    int flags = GETFLAGS(BlockType) >> 12;

    int index = 0;
    Block &block = Blocks[btype];

    //front
    MakeFace(verts, physicsBlockVertices, 0, index, block.pzTexInfo);
    index += 6;
    //right
    MakeFace(verts, physicsBlockVertices, 12, index, block.pxTexInfo);
    index += 6;
    //top

    MakeFace(verts, physicsBlockVertices, 24, index, block.pyTexInfo);
    index += 6;
    //left

    MakeFace(verts, physicsBlockVertices, 36, index, block.nxTexInfo);
    index += 6;
    //bottom

    MakeFace(verts, physicsBlockVertices, 48, index, block.nyTexInfo);
    index += 6;
    //back

    MakeFace(verts, physicsBlockVertices, 60, index, block.nzTexInfo);
    index += 6;

    _mesh = new PhysicsBlockMesh;
    pbmm->mesh = _mesh;
    gameToGl.enqueue(Message(GL_M_PHYSICSBLOCKMESH, (void *)pbmm));
}

PhysicsBlockBatch::~PhysicsBlockBatch()
{
    if (_mesh != NULL){
        PhysicsBlockMeshMessage *pbmm = new PhysicsBlockMeshMessage;
        pbmm->mesh = _mesh;
        gameToGl.enqueue(Message(GL_M_PHYSICSBLOCKMESH, (void *)pbmm));
    }
}

void PhysicsBlockBatch::draw(PhysicsBlockMesh *pbm, const f64v3 &PlayerPos, f32m4 &VP)
{
    if (pbm == NULL) return;
    if (pbm->numBlocks == 0) return;
    GlobalModelMatrix[3][0] = ((float)((double)pbm->bX - PlayerPos.x));
    GlobalModelMatrix[3][1] = ((float)((double)pbm->bY - PlayerPos.y + 0.5));
    GlobalModelMatrix[3][2] = ((float)((double)pbm->bZ - PlayerPos.z));

    glm::mat4 MVP = VP * GlobalModelMatrix;

    glUniformMatrix4fv(physicsBlockShader.mvpID, 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(physicsBlockShader.mID, 1, GL_FALSE, &GlobalModelMatrix[0][0]);

    // 1rst attribute buffer : vertices
    glBindBuffer(GL_ARRAY_BUFFER, pbm->vboID);
    glVertexAttribPointer(0, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(PhysicsBlockVertex), (void*)0);

    //UVs
    glVertexAttribPointer(1, 2, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(PhysicsBlockVertex), (void*)4);

    //textureAtlas_textureIndex
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(PhysicsBlockVertex), (void*)8);

    //textureDimensions
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(PhysicsBlockVertex), (void*)12);

    //normals
    glVertexAttribPointer(4, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PhysicsBlockVertex), (void*)16);

    //center
    glBindBuffer(GL_ARRAY_BUFFER, pbm->positionLightBufferID);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(PhysicsBlockPosLight), (void*)0);

    //color
    glVertexAttribPointer(6, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PhysicsBlockPosLight), (void*)12);

    //overlayColor
    glVertexAttribPointer(7, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PhysicsBlockPosLight), (void*)16);

    //light
    glVertexAttribPointer(8, 2, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(PhysicsBlockPosLight), (void*)20);

    glVertexAttribDivisor(0, 0); // particles vertices : always reuse the same vertices -> 0
    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);
    glVertexAttribDivisor(3, 0);
    glVertexAttribDivisor(4, 0);
    glVertexAttribDivisor(5, 1); // positions : one per cube
    glVertexAttribDivisor(6, 1); // color : one per cube
    glVertexAttribDivisor(7, 1); // color : one per cube
    glVertexAttribDivisor(8, 1); // light : one per cube

    //TODO: glDrawElementsInstanced
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, pbm->numBlocks); //draw instanced arrays
}

bool PhysicsBlockBatch::update(const deque < deque < deque < ChunkSlot* > > > &chunkList, double wX, double wY, double wZ)
{
    size_t i = 0;

    PhysicsBlockMeshMessage *pbmm = new PhysicsBlockMeshMessage;
    vector <PhysicsBlockPosLight> &verts = pbmm->posLight;
    verts.resize(physicsBlocks.size());
    
    ui8 color[3], overlayColor[3];

    //need to fix this so that color is correct
    Blocks[physicsBlocks[0].blockType].GetBlockColor(color, overlayColor, 0, 128, 128, Blocks[physicsBlocks[0].blockType].pzTexInfo);

    while (i < physicsBlocks.size()){
        if (physicsBlocks[i].update(chunkList, wX, wY, wZ)){
            physicsBlocks[i] = physicsBlocks.back();
            physicsBlocks.pop_back(); //dont need to increment i 
        } else{ //if it was successfully updated, add its data to the buffers
            verts[i].pos[0] = physicsBlocks[i].position.x - _bX;
            verts[i].pos[1] = physicsBlocks[i].position.y - _bY;
            verts[i].pos[2] = physicsBlocks[i].position.z - _bZ;

            verts[i].color[0] = color[0];
            verts[i].color[1] = color[1];
            verts[i].color[2] = color[2];
            verts[i].overlayColor[0] = overlayColor[0];
            verts[i].overlayColor[1] = overlayColor[1];
            verts[i].overlayColor[2] = overlayColor[2];

            verts[i].light[0] = physicsBlocks[i].light[0];
            verts[i].light[1] = physicsBlocks[i].light[1];
            i++;
        }
    }

    _numBlocks = i;
    verts.resize(_numBlocks); //chop off extras

    if (_numBlocks == 0){
        if (_mesh != NULL){
            pbmm->mesh = _mesh;
            gameToGl.enqueue(Message(GL_M_PHYSICSBLOCKMESH, (void *)pbmm));
            _mesh = NULL;
        }
        return 1;
    }

    pbmm->bX = _bX;
    pbmm->bY = _bY;
    pbmm->bZ = _bZ;
    pbmm->numBlocks = _numBlocks;
    if (_mesh == NULL){
        pError("AHHHHH WHAT? Physics block mesh null!?");
    }
    pbmm->mesh = _mesh;

    gameToGl.enqueue(Message(GL_M_PHYSICSBLOCKMESH, (void *)pbmm));

    return 0;
}

void PhysicsBlockBatch::addBlock(const glm::dvec3 &pos, int ydiff, glm::vec2 &dir, glm::vec3 extraForce)
{
    if (physicsBlocks.size() == 0){ //if its the first one, set it to this block
        _bX = pos.x;
        _bY = pos.y;
        _bZ = pos.z;
    }
    physicsBlocks.push_back(PhysicsBlock(pos, blockType, ydiff, dir, extraForce));
}