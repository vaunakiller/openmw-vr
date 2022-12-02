#include "frame.hpp"
#include "layer.hpp"

namespace VR
{

    ProjectionLayerView::ProjectionLayerView()
        : colorSwapchain()
        , depthSwapchain()
        , subImage()
        , view()
    {
    }

    ProjectionLayerView::~ProjectionLayerView()
    {
    }

    ProjectionLayer::ProjectionLayer()
        : views()
    {
    }

    ProjectionLayer::~ProjectionLayer()
    {
    }

    QuadLayer::QuadLayer()
    {
    }
    QuadLayer::~QuadLayer()
    {
    }

}
