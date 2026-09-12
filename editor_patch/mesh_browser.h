#pragma once

#include <windows.h>
#include <string>

enum AlpineMeshKind : unsigned
{
    ALPINE_MESH_V3M = 0x1,
    ALPINE_MESH_V3C = 0x2,
    ALPINE_MESH_VFX = 0x4,
    ALPINE_MESH_ANY = ALPINE_MESH_V3M | ALPINE_MESH_V3C | ALPINE_MESH_VFX,
};

// Opens the Alpine mesh browser over the meshes the editor can load (registered search paths
// plus mounted packfiles). `filename` seeds the selection and receives the chosen bare
// filename; `kinds` restricts the listing to the extensions the calling field accepts.
// `anim`, when given, adds the animation pane: it seeds that pane's selection and receives the
// chosen animation, and is left untouched unless the user picks one for a .v3c mesh.
// Returns false when the user cancels or the dialog resource is missing.
bool alpine_browse_mesh(HWND parent, std::string& filename, unsigned kinds = ALPINE_MESH_ANY,
                        std::string* anim = nullptr);

struct EditorVMesh;

// True when the named animation file can drive this character. An .rfa is indexed by the
// character's own bone index with no bounds check (0x005002DB), so one with fewer bones reads
// past its bone table; this also proves every offset the playback path follows is inside the file.
bool alpine_anim_playable_on(EditorVMesh* vmesh, const char* anim_name);
