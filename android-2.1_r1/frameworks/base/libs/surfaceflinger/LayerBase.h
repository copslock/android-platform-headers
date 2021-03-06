/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_LAYER_BASE_H
#define ANDROID_LAYER_BASE_H

#include <stdint.h>
#include <sys/types.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <private/ui/SharedBufferStack.h>
#include <private/ui/LayerState.h>

#include <utils/RefBase.h>

#include <ui/Region.h>
#include <ui/Overlay.h>

#include <pixelflinger/pixelflinger.h>

#include "Transform.h"

namespace android {

// ---------------------------------------------------------------------------

class DisplayHardware;
class Client;
class GraphicBuffer;
class GraphicPlane;
class SurfaceFlinger;

// ---------------------------------------------------------------------------

class LayerBase : public RefBase
{
    // poor man's dynamic_cast below
    template<typename T>
    struct getTypeInfoOfAnyType {
        static uint32_t get() { return T::typeInfo; }
    };

    template<typename T>
    struct getTypeInfoOfAnyType<T*> {
        static uint32_t get() { return getTypeInfoOfAnyType<T>::get(); }
    };

public:
    static const uint32_t typeInfo;
    static const char* const typeID;
    virtual char const* getTypeID() const { return typeID; }
    virtual uint32_t getTypeInfo() const { return typeInfo; }
    
    template<typename T>
    static T dynamicCast(LayerBase* base) {
        uint32_t mostDerivedInfo = base->getTypeInfo();
        uint32_t castToInfo = getTypeInfoOfAnyType<T>::get();
        if ((mostDerivedInfo & castToInfo) == castToInfo)
            return static_cast<T>(base);
        return 0;
    }

    
    LayerBase(SurfaceFlinger* flinger, DisplayID display);
    
    DisplayID           dpy;
    mutable bool        contentDirty;
            Region      visibleRegionScreen;
            Region      transparentRegionScreen;
            Region      coveredRegionScreen;
            
            struct State {
                uint32_t        w;
                uint32_t        h;
                uint32_t        requested_w;
                uint32_t        requested_h;
                uint32_t        z;
                uint8_t         alpha;
                uint8_t         flags;
                uint8_t         reserved[2];
                int32_t         sequence;   // changes when visible regions can change
                uint32_t        tint;
                Transform       transform;
                Region          transparentRegion;
            };

            // modify current state
            bool setPosition(int32_t x, int32_t y);
            bool setLayer(uint32_t z);
            bool setSize(uint32_t w, uint32_t h);
            bool setAlpha(uint8_t alpha);
            bool setMatrix(const layer_state_t::matrix22_t& matrix);
            bool setTransparentRegionHint(const Region& opaque);
            bool setFlags(uint8_t flags, uint8_t mask);
            
            void commitTransaction();
            bool requestTransaction();
            void forceVisibilityTransaction();
            
            uint32_t getTransactionFlags(uint32_t flags);
            uint32_t setTransactionFlags(uint32_t flags);
            
            Rect visibleBounds() const;
            void drawRegion(const Region& reg) const;

            void invalidate();
            
    /**
     * draw - performs some global clipping optimizations
     * and calls onDraw().
     * Typically this method is not overridden, instead implement onDraw()
     * to perform the actual drawing.  
     */
    virtual void draw(const Region& clip) const;
    
    /**
     * onDraw - draws the surface.
     */
    virtual void onDraw(const Region& clip) const = 0;
    
    /**
     * initStates - called just after construction
     */
    virtual void initStates(uint32_t w, uint32_t h, uint32_t flags);
    
    /**
     * doTransaction - process the transaction. This is a good place to figure
     * out which attributes of the surface have changed.
     */
    virtual uint32_t doTransaction(uint32_t transactionFlags);
    
    /**
     * setVisibleRegion - called to set the new visible region. This gives
     * a chance to update the new visible region or record the fact it changed.
     */
    virtual void setVisibleRegion(const Region& visibleRegion);
    
    /**
     * setCoveredRegion - called when the covered region changes. The covered
     * region correspond to any area of the surface that is covered 
     * (transparently or not) by another surface.
     */
    virtual void setCoveredRegion(const Region& coveredRegion);
    
    /**
     * validateVisibility - cache a bunch of things
     */
    virtual void validateVisibility(const Transform& globalTransform);

    /**
     * lockPageFlip - called each time the screen is redrawn and returns whether
     * the visible regions need to be recomputed (this is a fairly heavy
     * operation, so this should be set only if needed). Typically this is used
     * to figure out if the content or size of a surface has changed.
     */
    virtual void lockPageFlip(bool& recomputeVisibleRegions);
    
    /**
     * unlockPageFlip - called each time the screen is redrawn. updates the
     * final dirty region wrt the planeTransform.
     * At this point, all visible regions, surface position and size, etc... are
     * correct.
     */
    virtual void unlockPageFlip(const Transform& planeTransform, Region& outDirtyRegion);
    
    /**
     * finishPageFlip - called after all surfaces have drawn.
     */
    virtual void finishPageFlip();
    
    /**
     * needsBlending - true if this surface needs blending
     */
    virtual bool needsBlending() const  { return false; }

    /**
     * needsDithering - true if this surface needs dithering
     */
    virtual bool needsDithering() const { return false; }

    /**
     * transformed -- true is this surface needs a to be transformed
     */
    virtual bool transformed() const    { return mTransformed; }

    /**
     * isSecure - true if this surface is secure, that is if it prevents
     * screenshots or VNC servers.
     */
    virtual bool isSecure() const       { return false; }

    /** Called from the main thread, when the surface is removed from the
     * draw list */
    virtual status_t ditch() { return NO_ERROR; }

    /** called with the state lock when the surface is removed from the
     *  current list */
    virtual void onRemoved() { };
    
    
    enum { // flags for doTransaction()
        eVisibleRegion      = 0x00000002,
    };


    inline  const State&    drawingState() const    { return mDrawingState; }
    inline  const State&    currentState() const    { return mCurrentState; }
    inline  State&          currentState()          { return mCurrentState; }

    static int compareCurrentStateZ(
            sp<LayerBase> const * layerA,
            sp<LayerBase> const * layerB) {
        return layerA[0]->currentState().z - layerB[0]->currentState().z;
    }

    int32_t  getOrientation() const { return mOrientation; }
    int  tx() const             { return mLeft; }
    int  ty() const             { return mTop; }
    
protected:
    const GraphicPlane& graphicPlane(int dpy) const;
          GraphicPlane& graphicPlane(int dpy);

          GLuint createTexture() const;
    
          struct Texture {
              Texture() : name(-1U), width(0), height(0),
                  image(EGL_NO_IMAGE_KHR), transform(0), 
                  NPOTAdjust(false), dirty(true) { }
              GLuint        name;
              GLuint        width;
              GLuint        height;
              GLuint        potWidth;
              GLuint        potHeight;
              GLfloat       wScale;
              GLfloat       hScale;
              EGLImageKHR   image;
              uint32_t      transform;
              bool          NPOTAdjust;
              bool          dirty;
          };

          void clearWithOpenGL(const Region& clip, GLclampx r, GLclampx g,
                               GLclampx b, GLclampx alpha) const;
          void clearWithOpenGL(const Region& clip) const;
          void drawWithOpenGL(const Region& clip, const Texture& texture) const;
          void loadTexture(Texture* texture, 
                  const Region& dirty, const GGLSurface& t) const;
          status_t initializeEglImage(
                  const sp<GraphicBuffer>& buffer, Texture* texture);

          
                sp<SurfaceFlinger> mFlinger;
                uint32_t        mFlags;

                // cached during validateVisibility()
                bool            mTransformed;
                bool            mUseLinearFiltering;
                int32_t         mOrientation;
                GLfixed         mVertices[4][2];
                Rect            mTransformedBounds;
                int             mLeft;
                int             mTop;
            
                // these are protected by an external lock
                State           mCurrentState;
                State           mDrawingState;
    volatile    int32_t         mTransactionFlags;

                // don't change, don't need a lock
                bool            mPremultipliedAlpha;

                // atomic
    volatile    int32_t         mInvalidate;
                

protected:
    virtual ~LayerBase();

private:
    LayerBase(const LayerBase& rhs);
    void validateTexture(GLint textureName) const;
};


// ---------------------------------------------------------------------------

class LayerBaseClient : public LayerBase
{
public:
    class Surface;
   static const uint32_t typeInfo;
    static const char* const typeID;
    virtual char const* getTypeID() const { return typeID; }
    virtual uint32_t getTypeInfo() const { return typeInfo; }

    // lcblk is (almost) only accessed from the main SF thread, in the places
    // where it's not, a reference to Client must be held
    SharedBufferServer*     lcblk;

    LayerBaseClient(SurfaceFlinger* flinger, DisplayID display, 
            const sp<Client>& client, int32_t i);
    virtual ~LayerBaseClient();
    virtual void onFirstRef();

    const wp<Client>    client;

    inline  uint32_t    getIdentity() const { return mIdentity; }
    inline  int32_t     clientIndex() const { return mIndex; }
            int32_t     serverIndex() const;

   
            sp<Surface> getSurface();
    virtual sp<Surface> createSurface() const;
    
    virtual void onRemoved();

    class Surface : public BnSurface 
    {
    public:
        int32_t getToken() const { return mToken; }
        int32_t getIdentity() const { return mIdentity; }
        
    protected:
        Surface(const sp<SurfaceFlinger>& flinger, 
                SurfaceID id, int identity, 
                const sp<LayerBaseClient>& owner);
        virtual ~Surface();
        virtual status_t onTransact(uint32_t code, const Parcel& data,
                Parcel* reply, uint32_t flags);
        sp<LayerBaseClient> getOwner() const;

    private:
        virtual sp<GraphicBuffer> requestBuffer(int index, int usage);
        virtual status_t registerBuffers(const ISurface::BufferHeap& buffers); 
        virtual void postBuffer(ssize_t offset);
        virtual void unregisterBuffers();
        virtual sp<OverlayRef> createOverlay(uint32_t w, uint32_t h,
                int32_t format);

    protected:
        friend class LayerBaseClient;
        sp<SurfaceFlinger>  mFlinger;
        int32_t             mToken;
        int32_t             mIdentity;
        wp<LayerBaseClient> mOwner;
    };

    friend class Surface;

private:
                int32_t         mIndex;
    mutable     Mutex           mLock;
    mutable     wp<Surface>     mClientSurface;
    // only read
    const       uint32_t        mIdentity;
    static      int32_t         sIdentity;
};

// ---------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_LAYER_BASE_H
