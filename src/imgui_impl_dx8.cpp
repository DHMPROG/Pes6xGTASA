// Backend de rendu Dear ImGui pour Direct3D 8 (adapté de imgui_impl_dx9, v1.92).
#include "imgui_impl_dx8.h"
#include <d3d8.h>
#include <cstring>

struct ImGui_ImplDX8_Data {
    IDirect3DDevice8*       Device = nullptr;
    IDirect3DVertexBuffer8* VB = nullptr;
    IDirect3DIndexBuffer8*  IB = nullptr;
    int                     VertexBufferSize = 5000;
    int                     IndexBufferSize = 10000;
};

struct CUSTOMVERTEX {
    float    pos[3];
    D3DCOLOR col;
    float    uv[2];
};
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)
#define IMGUI_COL_TO_DX8_ARGB(_COL) (((_COL) & 0xFF00FF00) | (((_COL) & 0xFF0000) >> 16) | (((_COL) & 0xFF) << 16))

static ImGui_ImplDX8_Data* GetBackendData() {
    return ImGui::GetCurrentContext() ? (ImGui_ImplDX8_Data*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

static void SetSampler(IDirect3DDevice8* dev, D3DTEXTUREFILTERTYPE f) {
    dev->SetTextureStageState(0, D3DTSS_MINFILTER, f);
    dev->SetTextureStageState(0, D3DTSS_MAGFILTER, f);
}

static void SetupRenderState(ImDrawData* draw_data) {
    IDirect3DDevice8* dev = GetBackendData()->Device;

    D3DVIEWPORT8 vp;
    vp.X = vp.Y = 0;
    vp.Width = (DWORD)draw_data->DisplaySize.x;
    vp.Height = (DWORD)draw_data->DisplaySize.y;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    dev->SetViewport(&vp);

    dev->SetPixelShader(0);
    dev->SetVertexShader(D3DFVF_CUSTOMVERTEX);
    dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    dev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_CLIPPING, TRUE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    dev->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    SetSampler(dev, D3DTEXF_LINEAR);

    float L = draw_data->DisplayPos.x + 0.5f;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x + 0.5f;
    float T = draw_data->DisplayPos.y + 0.5f;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y + 0.5f;
    D3DMATRIX identity = { { { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 } } };
    D3DMATRIX projection = { { {
        2.0f / (R - L),      0.0f,                0.0f, 0.0f,
        0.0f,                2.0f / (T - B),      0.0f, 0.0f,
        0.0f,                0.0f,                0.5f, 0.0f,
        (L + R) / (L - R),   (T + B) / (B - T),   0.5f, 1.0f
    } } };
    dev->SetTransform(D3DTS_WORLD, &identity);
    dev->SetTransform(D3DTS_VIEW, &identity);
    dev->SetTransform(D3DTS_PROJECTION, &projection);
}

static void DrawCallback_ResetRenderState(const ImDrawList*, const ImDrawCmd*) {}
static void DrawCallback_SetSamplerLinear(const ImDrawList*, const ImDrawCmd*) { SetSampler(GetBackendData()->Device, D3DTEXF_LINEAR); }
static void DrawCallback_SetSamplerNearest(const ImDrawList*, const ImDrawCmd*) { SetSampler(GetBackendData()->Device, D3DTEXF_POINT); }

// Copie RGBA32 -> A8R8G8B8
static void CopyRegion(const ImU32* src, int src_pitch, unsigned char* dst, int dst_pitch, int w, int h) {
    for (int y = 0; y < h; y++) {
        const ImU32* s = (const ImU32*)((const unsigned char*)src + src_pitch * y);
        ImU32* d = (ImU32*)(dst + dst_pitch * y);
        for (int x = 0; x < w; x++) d[x] = IMGUI_COL_TO_DX8_ARGB(s[x]);
    }
}

// Textures en D3DPOOL_MANAGED : elles survivent à un Reset du device.
static void UpdateTexture(ImTextureData* tex) {
    ImGui_ImplDX8_Data* bd = GetBackendData();
    if (tex->Status == ImTextureStatus_WantCreate) {
        IDirect3DTexture8* dx_tex = nullptr;
        if (bd->Device->CreateTexture(tex->Width, tex->Height, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &dx_tex) < 0)
            return;
        D3DLOCKED_RECT lr;
        if (dx_tex->LockRect(0, &lr, nullptr, 0) == D3D_OK) {
            CopyRegion((const ImU32*)tex->GetPixels(), tex->Width * 4, (unsigned char*)lr.pBits, lr.Pitch, tex->Width, tex->Height);
            dx_tex->UnlockRect(0);
        }
        tex->SetTexID((ImTextureID)(intptr_t)dx_tex);
        tex->SetStatus(ImTextureStatus_OK);
    } else if (tex->Status == ImTextureStatus_WantUpdates) {
        IDirect3DTexture8* dx_tex = (IDirect3DTexture8*)(intptr_t)tex->TexID;
        for (ImTextureRect& r : tex->Updates) {
            RECT rc = { r.x, r.y, r.x + r.w, r.y + r.h };
            D3DLOCKED_RECT lr;
            if (dx_tex->LockRect(0, &lr, &rc, 0) == D3D_OK) {
                CopyRegion((const ImU32*)tex->GetPixelsAt(r.x, r.y), tex->Width * 4, (unsigned char*)lr.pBits, lr.Pitch, r.w, r.h);
                dx_tex->UnlockRect(0);
            }
        }
        tex->SetStatus(ImTextureStatus_OK);
    } else if (tex->Status == ImTextureStatus_WantDestroy) {
        if (IDirect3DTexture8* dx_tex = (IDirect3DTexture8*)(intptr_t)tex->TexID) {
            dx_tex->Release();
            tex->SetTexID(ImTextureID_Invalid);
        }
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

void ImGui_ImplDX8_RenderDrawData(ImDrawData* draw_data) {
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return;
    ImGui_ImplDX8_Data* bd = GetBackendData();
    IDirect3DDevice8* dev = bd->Device;

    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                UpdateTexture(tex);

    if (draw_data->TotalVtxCount == 0) return;

    if (!bd->VB || bd->VertexBufferSize < draw_data->TotalVtxCount) {
        if (bd->VB) { bd->VB->Release(); bd->VB = nullptr; }
        bd->VertexBufferSize = draw_data->TotalVtxCount + 5000;
        if (dev->CreateVertexBuffer(bd->VertexBufferSize * sizeof(CUSTOMVERTEX), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                                    D3DFVF_CUSTOMVERTEX, D3DPOOL_DEFAULT, &bd->VB) < 0)
            return;
    }
    if (!bd->IB || bd->IndexBufferSize < draw_data->TotalIdxCount) {
        if (bd->IB) { bd->IB->Release(); bd->IB = nullptr; }
        bd->IndexBufferSize = draw_data->TotalIdxCount + 10000;
        if (dev->CreateIndexBuffer(bd->IndexBufferSize * sizeof(ImDrawIdx), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                                   sizeof(ImDrawIdx) == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32, D3DPOOL_DEFAULT, &bd->IB) < 0)
            return;
    }

    // Sauvegarde de l'état du jeu (en DX8 le state block est capturé à la création)
    DWORD state_block = 0;
    if (dev->CreateStateBlock(D3DSBT_ALL, &state_block) < 0)
        return;
    D3DMATRIX last_world, last_view, last_projection;
    dev->GetTransform(D3DTS_WORLD, &last_world);
    dev->GetTransform(D3DTS_VIEW, &last_view);
    dev->GetTransform(D3DTS_PROJECTION, &last_projection);
    IDirect3DBaseTexture8* last_tex = nullptr;
    dev->GetTexture(0, &last_tex);
    IDirect3DVertexBuffer8* last_vb = nullptr; UINT last_stride = 0;
    dev->GetStreamSource(0, &last_vb, &last_stride);
    IDirect3DIndexBuffer8* last_ib = nullptr; UINT last_base = 0;
    dev->GetIndices(&last_ib, &last_base);

    CUSTOMVERTEX* vtx_dst;
    ImDrawIdx* idx_dst;
    if (bd->VB->Lock(0, draw_data->TotalVtxCount * sizeof(CUSTOMVERTEX), (BYTE**)&vtx_dst, D3DLOCK_DISCARD) < 0) {
        dev->DeleteStateBlock(state_block);
        return;
    }
    if (bd->IB->Lock(0, draw_data->TotalIdxCount * sizeof(ImDrawIdx), (BYTE**)&idx_dst, D3DLOCK_DISCARD) < 0) {
        bd->VB->Unlock();
        dev->DeleteStateBlock(state_block);
        return;
    }
    for (const ImDrawList* dl : draw_data->CmdLists) {
        const ImDrawVert* v = dl->VtxBuffer.Data;
        for (int i = 0; i < dl->VtxBuffer.Size; i++, v++, vtx_dst++) {
            vtx_dst->pos[0] = v->pos.x;
            vtx_dst->pos[1] = v->pos.y;
            vtx_dst->pos[2] = 0.0f;
            vtx_dst->col = IMGUI_COL_TO_DX8_ARGB(v->col);
            vtx_dst->uv[0] = v->uv.x;
            vtx_dst->uv[1] = v->uv.y;
        }
        memcpy(idx_dst, dl->IdxBuffer.Data, dl->IdxBuffer.Size * sizeof(ImDrawIdx));
        idx_dst += dl->IdxBuffer.Size;
    }
    bd->VB->Unlock();
    bd->IB->Unlock();
    dev->SetStreamSource(0, bd->VB, sizeof(CUSTOMVERTEX));

    SetupRenderState(draw_data);

    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (const ImDrawList* dl : draw_data->CmdLists) {
        for (int cmd_i = 0; cmd_i < dl->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &dl->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr) {
                if (pcmd->UserCallback == DrawCallback_ResetRenderState)
                    SetupRenderState(draw_data);
                else
                    pcmd->UserCallback(dl, pcmd);
                continue;
            }
            if (pcmd->ClipRect.z <= pcmd->ClipRect.x || pcmd->ClipRect.w <= pcmd->ClipRect.y)
                continue;
            dev->SetTexture(0, (IDirect3DTexture8*)pcmd->GetTexID());
            // En DX8 le « base vertex » se donne à SetIndices
            dev->SetIndices(bd->IB, pcmd->VtxOffset + global_vtx_offset);
            dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, (UINT)dl->VtxBuffer.Size,
                                      pcmd->IdxOffset + global_idx_offset, pcmd->ElemCount / 3);
        }
        global_idx_offset += dl->IdxBuffer.Size;
        global_vtx_offset += dl->VtxBuffer.Size;
    }

    // Restauration de l'état du jeu
    dev->SetTransform(D3DTS_WORLD, &last_world);
    dev->SetTransform(D3DTS_VIEW, &last_view);
    dev->SetTransform(D3DTS_PROJECTION, &last_projection);
    dev->ApplyStateBlock(state_block);
    dev->DeleteStateBlock(state_block);
    dev->SetTexture(0, last_tex);
    dev->SetStreamSource(0, last_vb, last_stride);
    dev->SetIndices(last_ib, last_base);
    if (last_tex) last_tex->Release();
    if (last_vb) last_vb->Release();
    if (last_ib) last_ib->Release();
}

void ImGui_ImplDX8_InvalidateDeviceObjects() {
    ImGui_ImplDX8_Data* bd = GetBackendData();
    if (!bd) return;
    if (bd->VB) { bd->VB->Release(); bd->VB = nullptr; }
    if (bd->IB) { bd->IB->Release(); bd->IB = nullptr; }
}

void ImGui_ImplDX8_NewFrame() {}

bool ImGui_ImplDX8_Init(IDirect3DDevice8* device) {
    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    ImGui_ImplDX8_Data* bd = IM_NEW(ImGui_ImplDX8_Data)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_dx8";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Renderer_TextureMaxWidth = pio.Renderer_TextureMaxHeight = 2048;
    pio.DrawCallback_ResetRenderState = DrawCallback_ResetRenderState;
    pio.DrawCallback_SetSamplerLinear = DrawCallback_SetSamplerLinear;
    pio.DrawCallback_SetSamplerNearest = DrawCallback_SetSamplerNearest;

    bd->Device = device;
    bd->Device->AddRef();
    return true;
}

void ImGui_ImplDX8_Shutdown() {
    ImGui_ImplDX8_Data* bd = GetBackendData();
    if (!bd) return;
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1) { tex->SetStatus(ImTextureStatus_WantDestroy); UpdateTexture(tex); }
    ImGui_ImplDX8_InvalidateDeviceObjects();
    if (bd->Device) bd->Device->Release();
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    ImGui::GetPlatformIO().ClearRendererHandlers();
    IM_DELETE(bd);
}
