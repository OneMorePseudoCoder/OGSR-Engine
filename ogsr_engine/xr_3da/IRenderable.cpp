#include "stdafx.h"

#include "../xrcdb/ispatial.h"

#include "irenderable.h"

IRenderable::IRenderable()
{
    renderable.xform.identity();
    renderable.visual = nullptr;
    renderable.pROS = nullptr;
    renderable.pROS_Allowed = TRUE;
    ISpatial* self = dynamic_cast<ISpatial*>(this);
    if (self)
        self->spatial.type |= STYPE_RENDERABLE;
}

extern ENGINE_API BOOL g_bRendering;
IRenderable::~IRenderable()
{
    VERIFY(!g_bRendering);
    Render->model_Delete(renderable.visual);
    if (renderable.pROS)
        Render->ros_destroy(renderable.pROS);
    renderable.visual = nullptr;
    renderable.pROS = nullptr;
}

IRender_ObjectSpecific* IRenderable::renderable_ROS()
{
    if (nullptr == renderable.pROS && renderable.pROS_Allowed)
        renderable.pROS = Render->ros_create(this);
    return renderable.pROS;
}
