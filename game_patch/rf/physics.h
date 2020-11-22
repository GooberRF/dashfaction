#pragma once

#include "common.h"

namespace rf
{
    struct ObjectCreateInfo;

    struct PCollisionOut
    {
        Vector3 hit_point;
        Vector3 normal;
        float fraction;
        int material_idx;
        int field_20;
        Vector3 obj_vel;
        int obj_handle;
        int texture;
        int field_38;
        void* face;
        int field_40;
    };
    static_assert(sizeof(PCollisionOut) == 0x44);

    struct PCollisionSphere
    {
        Vector3 center;
        float radius = 0.0f;
        float spring_const = 0.0f;
        int spring_length = 0;
    };
    static_assert(sizeof(PCollisionSphere) == 0x18);

    struct PhysicsData
    {
        float elasticity;
        float drag;
        float friction;
        int bouyancy;
        float mass;
        Matrix3 body_inv;
        Matrix3 tensor_inv;
        Vector3 pos;
        Vector3 next_pos;
        Matrix3 orient;
        Matrix3 next_orient;
        Vector3 vel;
        Vector3 rotvel;
        Vector3 field_D4;
        Vector3 field_E0;
        Vector3 rot_change_unk_delta;
        float radius;
        VArray<PCollisionSphere> cspheres;
        Vector3 bbox_min;
        Vector3 bbox_max;
        int flags;
        int flags2;
        float frame_time;
        PCollisionOut collide_out;
    };
    static_assert(sizeof(PhysicsData) == 0x170);

    static auto& PhysicsCreateObject = AddrAsRef<void(PhysicsData *pd, ObjectCreateInfo *oci)>(0x0049EC90);
    static auto& PhysicsDeleteObject = AddrAsRef<void(PhysicsData *pd)>(0x0049F1D0);
    static auto& VArray_PCollisionSphere__Add = AddrAsRef<int __thiscall(VArray<PCollisionSphere> *this_, PCollisionSphere csphere)>(0x00417F30);
}