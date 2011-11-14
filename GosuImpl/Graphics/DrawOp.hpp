#ifndef GOSUIMPL_DRAWOP_HPP
#define GOSUIMPL_DRAWOP_HPP

#include <Gosu/GraphicsBase.hpp>
#include <Gosu/Color.hpp>
#include <GosuImpl/Graphics/Common.hpp>
#include <GosuImpl/Graphics/TexChunk.hpp>
#include <cassert>

namespace Gosu
{
    // This looks like it may include a RenderStateDescriptor later.
    struct DrawOp
    {
        ZPos z;
        
        Transform* transform;
        ClipRect clipRect;
        
        struct Vertex
        {
            float x, y;
            Color c;
            Vertex() {}
            Vertex(float x, float y, Color c) : x(x), y(y), c(c) {}
        };
        
        Vertex vertices[4];
        // Number of vertices used, or: complement index of code block
        int verticesOrBlockIndex;
        const TexChunk* chunk;
        AlphaMode mode;
        
        RenderStateDescriptor impliedRenderState() const
        {
            assert (verticesOrBlockIndex == 4);
            
            RenderStateDescriptor rsd;
            rsd.texName = chunk ? chunk->texName() : NO_TEXTURE;
            rsd.transform = transform;
            rsd.clipRect = clipRect;
            rsd.mode = mode;
            return rsd;
        }
        
        DrawOp(Transform& transform, int verticesOrBlockIndex = 4)
        : transform(&transform), verticesOrBlockIndex(verticesOrBlockIndex), chunk(0)
        {
            clipRect.width = NO_CLIPPING;
        }
        
        void perform(RenderState& current, const DrawOp* next) const
        {
            // This should not be called on GL code ops.
            assert (verticesOrBlockIndex >= 2);
            assert (verticesOrBlockIndex <= 4);
            
            #ifdef GOSU_IS_IPHONE
            static const unsigned MAX_AUTOGROUP = 24;
            
            static int spriteCounter = 0;
            static float spriteVertices[12 * MAX_AUTOGROUP];
            static float spriteTexcoords[12 * MAX_AUTOGROUP];
            static unsigned spriteColors[6 * MAX_AUTOGROUP];
            
            // iPhone specific setup
            static bool isSetup = false;
            if (!isSetup)
            {
                // Sets up pointers and enables states needed for using vertex arrays and textures
                glVertexPointer(2, GL_FLOAT, 0, spriteVertices);
                glEnableClientState(GL_VERTEX_ARRAY);
                glTexCoordPointer(2, GL_FLOAT, 0, spriteTexcoords);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                // TODO: See if I can somehow change the format of the color pointer, or maybe change the internal color representation on iOS.
                glColorPointer(4, GL_UNSIGNED_BYTE, 0, spriteColors);
                glEnableClientState(GL_COLOR_ARRAY);
                
                isSetup = true;
            }
            #endif
            
            current.setRenderState(impliedRenderState());
            
            if (chunk)
            {
                #ifdef GOSU_IS_IPHONE
                float left, top, right, bottom;
                chunk->getCoords(left, top, right, bottom);
                spriteTexcoords[spriteCounter*12 + 0] = left;
                spriteTexcoords[spriteCounter*12 + 1] = top;
                spriteTexcoords[spriteCounter*12 + 2] = right;
                spriteTexcoords[spriteCounter*12 + 3] = top;
                spriteTexcoords[spriteCounter*12 + 4] = left;
                spriteTexcoords[spriteCounter*12 + 5] = bottom;
                
                spriteTexcoords[spriteCounter*12 + 6] = right;
                spriteTexcoords[spriteCounter*12 + 7] = top;
                spriteTexcoords[spriteCounter*12 + 8] = left;
                spriteTexcoords[spriteCounter*12 + 9] = bottom;
                spriteTexcoords[spriteCounter*12 + 10] = right;
                spriteTexcoords[spriteCounter*12 + 11] = bottom;
                #endif
            }

            #ifndef GOSU_IS_IPHONE
            if (verticesOrBlockIndex == 2)
                glBegin(GL_LINES);
            else if (verticesOrBlockIndex == 3)
                glBegin(GL_TRIANGLES);
            else // if (verticesOrBlockIndex == 4)
                glBegin(GL_QUADS);

            float left, top, right, bottom;
            if (chunk)
                chunk->getCoords(left, top, right, bottom);
            
            for (unsigned i = 0; i < verticesOrBlockIndex; i++)
            {
                glColor4ubv(reinterpret_cast<const GLubyte*>(&vertices[i].c));
                if (chunk)
                    switch (i)
                    {
                    case 0:
                        glTexCoord2f(left, top);
                        break;
                    case 1:
                        glTexCoord2f(right, top);
                        break;
                    case 2:
                        glTexCoord2f(right, bottom);
                        break;
                    case 3:
                        glTexCoord2f(left, bottom);
                        break;
                    }
                glVertex2f(vertices[i].x, vertices[i].y);
            }
            
            glEnd();
            #else
            for (int i = 0; i < 3; ++i)
            {
                spriteVertices[spriteCounter*12 + i*2] = vertices[i].x;
                spriteVertices[spriteCounter*12 + i*2+1] = vertices[i].y;
                spriteColors[spriteCounter*6 + i] = vertices[i].c.abgr();
            }
            for (int i = 0; i < 3; ++i)
            {
                spriteVertices[spriteCounter*12 + 6 + i*2] = vertices[i + 1].x;
                spriteVertices[spriteCounter*12 + 6 + i*2+1] = vertices[i + 1].y;
                spriteColors[spriteCounter*6 + 3 + i] = vertices[i + 1].c.abgr();
            }
            
            ++spriteCounter;
            if (spriteCounter == MAX_AUTOGROUP || next == 0 ||
                chunk == 0 || next->chunk == 0 || next->transform != transform ||
                next->chunk->texName() != chunk->texName() || next->mode != mode ||
                !(clipRect == next->clipRect))
            {
                glDrawArrays(GL_TRIANGLES, 0, 6 * spriteCounter);
                //if (spriteCounter > 1)
                //    printf("grouped %d quads\n", spriteCounter);
                spriteCounter = 0;
            }
            #endif
        }
        
        void compileTo(VertexArrays& vas) const
        {
            RenderStateDescriptor rsd = impliedRenderState();
            if (vas.empty() || !(vas.front().renderState == rsd))
            {
                //if (!vas.empty())
                //    puts("Oh no!! Splitting up vertex array!!"); fflush(0);
                vas.push_back(VertexArray());
                vas.back().renderState = rsd;
            }
            
            ArrayVertex result[4];
            for (int i = 0; i < 4; ++i)
            {
                result[i].vertices[0] = vertices[i].x;
                result[i].vertices[1] = vertices[i].y;
                result[i].vertices[2] = 0;
                result[i].color = vertices[i].c.abgr();
            }
            float left, top, right, bottom;
            chunk->getCoords(left, top, right, bottom);
            result[0].texCoords[0] = left, result[0].texCoords[1] = top;
            result[1].texCoords[0] = right, result[1].texCoords[1] = top;
            result[2].texCoords[0] = right, result[2].texCoords[1] = bottom;
            result[3].texCoords[0] = left, result[3].texCoords[1] = bottom;
            
            vas.back().vertices.insert(vas.back().vertices.end(), result, result + 4);
        }
        
        bool operator<(const DrawOp& other) const
        {
            return z < other.z;
        }
    };
}

#endif
