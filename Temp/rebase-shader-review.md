## Snapshot and result

PASS: no reachable rebase-specific shader or CPU/GPU contract defect found. Reviewed committed HEAD 1c3608533ba58d1a1327b51928ed4dced5426c53 against old HEAD d170b18b843a3a96af227cef467b824b4aa214c5, with main cd6d410c064a5a5f905bf68efaf28493ab6fc33a and old base b56f20fa44d0424c2fdf5003157563684ed747f5 separating inherited foundation behavior from incoming camera changes. Git status was clean throughout the source review; no propagation edits were observed.

## Coverage

The documented Get-SessionChangeInventory invocation returned status pass and truncated false. Its
shader review set comprises ShaderFunctions.h, ShaderMainLayout.h, Water/Water.frag, and
Water/WaterNormalSampling.h. Reviewed every changed region and surrounding function. The latter has
only a framing terminology comment change. Read shader root and Water contracts and the skill
footgun reference. No changed shader exceeds 5,000 bt-token-v1: measured totals respectively 2393,
1572, 2643, and 1554.

ShaderFunctions.h imports main's ToEyeLine unchanged. Orthographic flight-plane sampling follows the published direction; perspective sampling retains eye-minus-position. Traced callers in Terrain.frag, ModelForward.h, Water.frag, and LightingFixture.frag. No new executable matrix inverse, descriptor indexing, synchronization, optional stage, or extension behavior occurs in the changed regions.

ShaderMainLayout.h adds one vec4 water eye and assigns the existing direction's w component as the
projection flag. ShaderLayoutsBase.h includes the shared declaration and establishes scalar
uniforms; CPU vec4 derives from XMFLOAT4. All following members therefore move together on both
surfaces. BufferManager allocates sizeof(shaders::MainLayout), TextureDescriptors binds that
allocation at kiGlobalBindingMainUniform with VK_WHOLE_SIZE, and both application PCH paths include
their ShaderLayouts.h wrapper.

Existing shader include paths remain intact: ShaderLayouts.h reaches ShaderLayoutsBase.h and ShaderMainLayout.h; changed entry points and fixture/model consumers include ShaderFunctions.h through their existing includes. DataPacker ExportShader.cpp still emits -MD/-MF, parses the dependency file in CaptureDependencies, fingerprints input-root-relative dependencies, and compares fingerprints to invalidate changed dependencies. No include graph was removed by these rebase edits.

## Camera publication

CameraBase::CalculateMatricesAndVisibleArea retains main's framing-derived eye, FOV/orthographic projection, and water eye in the ordinary path. The comparison branch then replaces eye, target, direction, view, projection, and water eye using the explicit fixture pose; it forces mbOrthographicProjection false. LightingFixture::MakeCamera supplies XM_PIDIV4 and explicit clip distances. The removed WaterReflectionProjection implementation stays absent.

PopulateMainCamera writes both world and model projection matrices, actual eye, water eye, and the
direction/orthographic flag. Both ordinary and missing-camera-interpolate return paths call it.
Fixture camera refresh invokes the same camera calculation; Graphics::RenderMainPresentAcquire calls
RenderFrameMain before submission. Lighting::RecordPreparation copies the same main uniform buffer.
Fixture vertex projection and fragment smoke therefore consume the uploaded comparison perspective
contract. ModelViewer's independent RenderFrameMain writes the corresponding new fields while
preserving its separate model camera.

Water.frag uses the framing water eye in the existing camera-relative eye calculation; incoming main
and the rebase agree on that new field. The pre-existing top-down approximation in explicit
comparison water rendering was not introduced by this rebase. Global/Lighting/Water uniform tuning
uses GetRenderFraming, which retains ordinary framing and the foundation's comparison eye-height
tuning. ModelForward's material view vector is unchanged from incoming main and old foundation; this
review did not reopen that inherited behavior.

## Scope and limits

Authorization, minimality, and KISS passes: PASS against the supplied instruction to preserve main's
camera controls, retain explicit fixed-perspective comparison cameras, and keep legacy lighting
removed. No new speculative shader path was introduced. No external claim verification is needed for
this result. ShaderMainLayout.h requires the sibling /repo-code-review domain pass. No compile or
runtime was performed, as directed; this is a source-contract review, not shader generation or
rendering evidence.
