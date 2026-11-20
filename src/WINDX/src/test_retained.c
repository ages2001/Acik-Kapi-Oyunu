#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <d3drm.h>
#include "test_output.h"

static const char triangle_x[] =
    "xof 0302txt 0032\nHeader { 1; 0; 1; }\n"
    "Mesh Triangle { 3; 0.0;0.0;0.0;, 1.0;0.0;0.0;, 0.0;2.0;0.0;; 1; 3;0,1,2;;\n"
    " MeshMaterialList { 1;1;0; Material { 0.25;0.5;1.0;1.0;; 8.0; 0.0;0.0;0.0;; 0.0;0.0;0.0;; } }\n"
    " MeshNormals { 1; 0.0;0.0;1.0;; 1; 3;0,0,0;; }\n"
    " MeshTextureCoords { 3; 0.0;0.0;, 1.0;0.0;, 0.0;1.0;; }\n}\n";

static BOOL checked(HRESULT hr, const char *call)
{
    if (SUCCEEDED(hr)) return TRUE;
    report(call); report(" failed, HRESULT "); report_hex((DWORD)hr); report("\r\n");
    return FALSE;
}
#define TRY(call) do { if (!checked((call), #call)) goto done; } while (0)
#define REQUIRE(condition, why) do { if (!(condition)) { report(why "\r\n"); goto done; } } while (0)

static BOOL close_to(float a, float b) { return a >= b - 0.0001f && a <= b + 0.0001f; }
static BOOL point(const D3DVECTOR *v, float x, float y, float z)
{
    return close_to(v->x,x) && close_to(v->y,y) && close_to(v->z,z);
}

static BOOL test_scene(IDirect3DRM *api)
{
    IDirect3DRMFrame *root = NULL, *child = NULL, *element = NULL;
    IDirect3DRMFrameArray *children = NULL;
    IDirect3DRMMeshBuilder *builder = NULL;
    IDirect3DRMMesh *mesh = NULL;
    IDirect3DRMVisualArray *visuals = NULL;
    IDirect3DRMVisual *visual = NULL;
    D3DRMLOADMEMORY source = {(void *)triangle_x, sizeof(triangle_x) - 1};
    D3DVECTOR p = {1,1,1}, world;
    D3DRMVERTEX vertices[3];
    unsigned vcount = 0, fcount = 0, per_face = 0, indices[3], mask = 0;
    DWORD data_count = 0;
    BOOL ok = FALSE;
    TRY(IDirect3DRM_CreateFrame(api, NULL, &root));
    TRY(IDirect3DRM_CreateFrame(api, root, &child));
    TRY(IDirect3DRMFrame_GetChildren(root, &children));
    REQUIRE(IDirect3DRMFrameArray_GetSize(children)==1, "Expected one child in root");
    TRY(IDirect3DRMFrameArray_GetElement(children, 0, &element));
    REQUIRE(element == child, "Unexpected frame in child array");
    TRY(IDirect3DRMFrame_AddTranslation(root, D3DRMCOMBINE_REPLACE, 10,-4,2));
    TRY(IDirect3DRMFrame_AddScale(child, D3DRMCOMBINE_REPLACE, 2,3,4));
    TRY(IDirect3DRMFrame_AddTranslation(child, D3DRMCOMBINE_BEFORE, 1,2,3));
    TRY(IDirect3DRMFrame_Transform(child, &world, &p));
    REQUIRE(point(&world,14,5,18), "Scale, translation and parent composition mismatch");
    TRY(IDirect3DRMFrame_InverseTransform(child, &world, &world));
    REQUIRE(point(&world,1,1,1), "Inverse scale/translation mismatch");
    TRY(IDirect3DRMFrame_AddRotation(child, D3DRMCOMBINE_REPLACE, 0,0,2,1.57079632679f));
    TRY(IDirect3DRMFrame_AddTranslation(child, D3DRMCOMBINE_AFTER, 1,2,3));
    p = (D3DVECTOR){1,0,0};
    TRY(IDirect3DRMFrame_Transform(child, &world, &p));
    REQUIRE(point(&world,11,-1,5), "Rotation and translation mismatch");
    TRY(IDirect3DRMFrame_InverseTransform(child, &world, &world));
    REQUIRE(point(&world,1,0,0), "Inverse rotation mismatch");
    report("Frame tree, before/after transforms and inverse points PASS\r\n");

    TRY(IDirect3DRM_CreateMeshBuilder(api, &builder));
    TRY(IDirect3DRMMeshBuilder_Load(builder, &source, NULL, D3DRMLOAD_FROMMEMORY, NULL, NULL));
    REQUIRE(IDirect3DRMMeshBuilder_GetVertexCount(builder)==3 &&
            IDirect3DRMMeshBuilder_GetFaceCount(builder)==1, "Unexpected .x builder counts");
    TRY(IDirect3DRMMeshBuilder_CreateMesh(builder,&mesh));
    REQUIRE(IDirect3DRMMesh_GetGroupCount(mesh)==1, "Expected one .x material group");
    TRY(IDirect3DRMMesh_GetGroup(mesh,0,&vcount,&fcount,&per_face,&data_count,NULL));
    REQUIRE(vcount==3 && fcount==1 && per_face==3 && data_count==3, "Unexpected triangle group layout");
    TRY(IDirect3DRMMesh_GetGroup(mesh,0,NULL,NULL,NULL,&data_count,indices));
    TRY(IDirect3DRMMesh_GetVertices(mesh,0,0,3,vertices));
    for (unsigned i=0; i<3; ++i) {
        REQUIRE(indices[i]<3, "Group returned invalid vertex index");
        const D3DRMVERTEX *v = &vertices[indices[i]];
        REQUIRE(point(&v->normal,0,0,1), "Loaded vertex normal mismatch");
        if (point(&v->position,0,0,0) && close_to(v->tu,0) && close_to(v->tv,0)) mask |= 1;
        else if (point(&v->position,1,0,0) && close_to(v->tu,1) && close_to(v->tv,0)) mask |= 2;
        else if (point(&v->position,0,2,0) && close_to(v->tu,0) && close_to(v->tv,1)) mask |= 4;
    }
    REQUIRE(mask==7, "Indexed .x position/texture-coordinate mismatch");
    TRY(IDirect3DRMFrame_AddVisual(child,(IDirect3DRMVisual *)mesh));
    TRY(IDirect3DRMFrame_GetVisuals(child,&visuals));
    REQUIRE(IDirect3DRMVisualArray_GetSize(visuals)==1, "Expected one visual on the child");
    TRY(IDirect3DRMVisualArray_GetElement(visuals,0,&visual));
    REQUIRE(visual==(IDirect3DRMVisual *)mesh, "Frame visual differs from loaded mesh");
    report(".x triangle, material group, face indices, vertex positions, normals, UVs and attached mesh PASS\r\n");
    ok = TRUE;
done:
    if (visual) IDirect3DRMVisual_Release(visual);
    if (visuals) IDirect3DRMVisualArray_Release(visuals);
    if (mesh) IDirect3DRMMesh_Release(mesh);
    if (builder) IDirect3DRMMeshBuilder_Release(builder);
    if (element) IDirect3DRMFrame_Release(element);
    if (children) IDirect3DRMFrameArray_Release(children);
    if (child) IDirect3DRMFrame_Release(child);
    if (root) IDirect3DRMFrame_Release(root);
    return ok;
}

static HMODULE open_runtime(const char *name)
{
    char path[MAX_PATH], *tail;
#ifdef WINDX_REFERENCE_RUNTIME
    DWORD size = GetSystemDirectoryA(path,sizeof(path));
    if (!size || size >= sizeof(path)-1) return NULL;
    tail = path + size;
    *tail++ = '\\';
#else
    DWORD size = GetModuleFileNameA(NULL,path,sizeof(path));
    if (!size || size>=sizeof(path)) return NULL;
    tail=path;
    for (char *p=path; *p; ++p) if (*p=='\\' || *p=='/') tail=p+1;
#endif
    if ((SIZE_T)(tail-path) + lstrlenA(name) + 1 > sizeof(path)) return NULL;
    lstrcpyA(tail,name);
    HMODULE module=LoadLibraryA(path);
    if (!module) {
        DWORD err=GetLastError(); report("Could not load "); report(path); report(" error "); report_hex(err);
    } else {
        report(name); report(" loaded from ");
        if (GetModuleFileNameA(module,path,sizeof(path))) report(path);
    }
    report("\r\n");
    return module;
}

void WINAPI retained_entry(void)
{
    typedef HRESULT (WINAPI *create_fn)(IDirect3DRM **);
    initialize_test();
    HMODULE xof = open_runtime("d3dxof.dll");
    HMODULE rm = open_runtime("d3drm.dll");
    create_fn create = rm ? (create_fn)GetProcAddress(rm,"Direct3DRMCreate") : NULL;
    IDirect3DRM *api = NULL;
    BOOL ok = xof && create && checked(create(&api),"Direct3DRMCreate") && test_scene(api);
    if (api) IDirect3DRM_Release(api);
    if (rm) FreeLibrary(rm);
    if (xof) FreeLibrary(xof);
    ExitProcess(ok ? 0 : 1);
}
