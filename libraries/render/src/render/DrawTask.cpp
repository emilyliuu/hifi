//
//  DrawTask.cpp
//  render/src/render
//
//  Created by Sam Gateau on 5/21/15.
//  Copyright 20154 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DrawTask.h"

#include <algorithm>
#include <assert.h>

#include <PerfStat.h>
#include <ViewFrustum.h>
#include <gpu/Context.h>


using namespace render;

void render::cullItems(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext, const ItemIDsBounds& inItems, ItemIDsBounds& outItems) {
    assert(renderContext->getArgs());
    assert(renderContext->getArgs()->_viewFrustum);

    RenderArgs* args = renderContext->getArgs();
    auto renderDetails = renderContext->getArgs()->_details._item;

    renderDetails->_considered += inItems.size();
    
    // Culling / LOD
    for (auto item : inItems) {
        if (item.bounds.isNull()) {
            outItems.emplace_back(item); // One more Item to render
            continue;
        }

        // TODO: some entity types (like lights) might want to be rendered even
        // when they are outside of the view frustum...
        bool outOfView;
        {
            PerformanceTimer perfTimer("boxInFrustum");
            outOfView = args->_viewFrustum->boxInFrustum(item.bounds) == ViewFrustum::OUTSIDE;
        }
        if (!outOfView) {
            bool bigEnoughToRender;
            {
                PerformanceTimer perfTimer("shouldRender");
                bigEnoughToRender = (args->_shouldRender) ? args->_shouldRender(args, item.bounds) : true;
            }
            if (bigEnoughToRender) {
                outItems.emplace_back(item); // One more Item to render
            } else {
                renderDetails->_tooSmall++;
            }
        } else {
            renderDetails->_outOfView++;
        }
    }
    renderDetails->_rendered += outItems.size();
}


void FetchItems::run(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext, ItemIDsBounds& outItems) {
    auto& scene = sceneContext->_scene;
    auto& items = scene->getMasterBucket().at(_filter);

    outItems.clear();
    outItems.reserve(items.size());
    for (auto id : items) {
        auto& item = scene->getItem(id);
        outItems.emplace_back(ItemIDAndBounds(id, item.getBound()));
    }

    if (_probeNumItems) {
        _probeNumItems(renderContext, (int)outItems.size());
    }
}

void CullItems::run(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext, const ItemIDsBounds& inItems, ItemIDsBounds& outItems) {

    outItems.clear();
    outItems.reserve(inItems.size());
    RenderArgs* args = renderContext->getArgs();
    args->_details.pointTo(RenderDetails::OTHER_ITEM);
    cullItems(sceneContext, renderContext, inItems, outItems);
}

void CullItemsOpaque::run(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext, const ItemIDsBounds& inItems, ItemIDsBounds& outItems) {

    outItems.clear();
    outItems.reserve(inItems.size());
    RenderArgs* args = renderContext->getArgs();
    args->_details.pointTo(RenderDetails::OPAQUE_ITEM);
    cullItems(sceneContext, renderContext, inItems, outItems);
}

void CullItemsTransparent::run(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext, const ItemIDsBounds& inItems, ItemIDsBounds& outItems) {

    outItems.clear();
    outItems.reserve(inItems.size());
    RenderArgs* args = renderContext->getArgs();
    args->_details.pointTo(RenderDetails::TRANSLUCENT_ITEM);
    cullItems(sceneContext, renderContext, inItems, outItems);
}


struct ItemBound {
    float _centerDepth = 0.0f;
    float _nearDepth = 0.0f;
    float _farDepth = 0.0f;
    ItemID _id = 0;
    AABox _bounds;

    ItemBound() {}
    ItemBound(float centerDepth, float nearDepth, float farDepth, ItemID id, const AABox& bounds) : _centerDepth(centerDepth), _nearDepth(nearDepth), _farDepth(farDepth), _id(id), _bounds(bounds) {}
};

struct FrontToBackSort {
    bool operator() (const ItemBound& left, const ItemBound& right) {
        return (left._centerDepth < right._centerDepth);
    }
};

struct BackToFrontSort {
    bool operator() (const ItemBound& left, const ItemBound& right) {
        return (left._centerDepth > right._centerDepth);
    }
};

void render::depthSortItems(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext, bool frontToBack, const ItemIDsBounds& inItems, ItemIDsBounds& outItems) {
    assert(renderContext->getArgs());
    assert(renderContext->getArgs()->_viewFrustum);
    
    auto& scene = sceneContext->_scene;
    RenderArgs* args = renderContext->getArgs();
    

    // Allocate and simply copy
    outItems.clear();
    outItems.reserve(inItems.size());


    // Make a local dataset of the center distance and closest point distance
    std::vector<ItemBound> itemBounds;
    itemBounds.reserve(outItems.size());

    for (auto itemDetails : inItems) {
        auto item = scene->getItem(itemDetails.id);
        auto bound = itemDetails.bounds; // item.getBound();
        float distance = args->_viewFrustum->distanceToCamera(bound.calcCenter());

        itemBounds.emplace_back(ItemBound(distance, distance, distance, itemDetails.id, bound));
    }

    // sort against Z
    if (frontToBack) {
        FrontToBackSort frontToBackSort;
        std::sort (itemBounds.begin(), itemBounds.end(), frontToBackSort); 
    } else {
        BackToFrontSort  backToFrontSort;
        std::sort (itemBounds.begin(), itemBounds.end(), backToFrontSort); 
    }

    // FInally once sorted result to a list of itemID
    for (auto& itemBound : itemBounds) {
       outItems.emplace_back(ItemIDAndBounds(itemBound._id, itemBound._bounds));
    }
}


void DepthSortItems::run(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext, const ItemIDsBounds& inItems, ItemIDsBounds& outItems) {
    outItems.clear();
    outItems.reserve(inItems.size());
    depthSortItems(sceneContext, renderContext, _frontToBack, inItems, outItems);
}

void render::renderItems(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext, const ItemIDsBounds& inItems, int maxDrawnItems) {
    auto& scene = sceneContext->_scene;
    RenderArgs* args = renderContext->getArgs();
    // render
    if ((maxDrawnItems < 0) || (maxDrawnItems > (int) inItems.size())) {
        for (auto itemDetails : inItems) {
            auto item = scene->getItem(itemDetails.id);
            item.render(args);
        }
    } else {
        int numItems = 0;
        for (auto itemDetails : inItems) {
            auto item = scene->getItem(itemDetails.id);
            if (numItems + 1 >= maxDrawnItems) {
                item.render(args);
                return;
            }
            item.render(args);
            numItems++;
            if (numItems >= maxDrawnItems) {
                return;
            }
        }
    }
}

void DrawLight::run(const SceneContextPointer& sceneContext, const RenderContextPointer& renderContext) {
    assert(renderContext->getArgs());
    assert(renderContext->getArgs()->_viewFrustum);

    // render lights
    auto& scene = sceneContext->_scene;
    auto& items = scene->getMasterBucket().at(ItemFilter::Builder::light());


    ItemIDsBounds inItems;
    inItems.reserve(items.size());
    for (auto id : items) {
        auto item = scene->getItem(id);
        inItems.emplace_back(ItemIDAndBounds(id, item.getBound()));
    }

    ItemIDsBounds culledItems;
    culledItems.reserve(inItems.size());
    RenderArgs* args = renderContext->getArgs();
    args->_details.pointTo(RenderDetails::OTHER_ITEM);
    cullItems(sceneContext, renderContext, inItems, culledItems);

    gpu::doInBatch(args->_context, [=](gpu::Batch& batch) {
        args->_batch = &batch;
        renderItems(sceneContext, renderContext, culledItems);
    });
    args->_batch = nullptr;
}
