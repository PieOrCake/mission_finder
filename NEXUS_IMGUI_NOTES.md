# Nexus API and ImGui – Addon Drawing Notes

## What the Nexus API says (Nexus.h)

- **Renderer.Register(ERenderType, GUI_RENDER callback)**  
  Registers a render callback. `ERenderType` is one of PreRender, Render, PostRender, OptionsRender. The callback has signature `void func()` (no arguments).

- **AddonAPI provides:**  
  `ImguiContext` (void*), `ImguiMalloc`, `ImguiFree`. So the loader gives addons an ImGui context and allocator to use.

- **Intended use:**  
  Addons are expected to draw with ImGui using the context and allocator provided by the loader.

## Other addons (nexus-rs example)

- **nexus-rs (Rust)** example addon:  
  https://github.com/Zerthox/nexus-rs/blob/main/nexus_example_addon/src/lib.rs  

  They call `register_render(RenderType::Render, render!(|ui| { ... }))`. Inside the callback they draw ImGui (`Window::new("Test window").build(ui, || { ui.button("show"); ... })`). The closure receives `ui` (ImGui), so the Rust bindings (and likely the loader) set up the ImGui context before invoking the callback.

- **Conclusion:**  
  The loader probably sets the current ImGui context (and maybe allocator) before calling each addon’s render callback, so the callback can draw with ImGui directly.

## Our situation (Mission Finder)

- We use **FetchContent** to build our **own** ImGui (v1.90) and link it into our addon DLL.
- The loader has its **own** ImGui build and passes `APIDefs->ImguiContext` (and allocator) from that build.
- When we call `ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext)` we are passing a **context created by the loader’s ImGui** into **our ImGui’s** `SetCurrentContext`. Any subsequent `ImGui::Begin`, etc., run in **our** ImGui code using that context.
- If the loader’s ImGui and our ImGui differ (version, build options, ABI), **ImGuiContext** layout can differ. Using the loader’s context with our ImGui code can then cause crashes or corruption when we draw.

## What we tried

- Setting the context only in **AddonLoad**: load is fine, but drawing in the render callback still crashes.
- Setting the context at the **start of the render callback**: still crashes when we call `ImGui::Begin` (launcher or main window).
- **No** ImGui calls in the callback (only Quick Access registration): **no crash**. So the crash is tied to **any** ImGui draw from our callback.

So the failure is consistent with an **ImGui ABI/version mismatch**: we are using the loader’s context with our own ImGui build.

## Recommended direction

1. **Use the same ImGui as the loader**  
   Do **not** compile ImGui into the addon. Instead:
   - Use a **shared** ImGui build (e.g. **lib_imgui** from gw2-addon-loader if Nexus uses it), or  
   - Get ImGui (headers + lib or DLL) from the Nexus/Raidcore distribution and link against that so the addon and loader share one ImGui build and one `ImGuiContext` layout.

2. **Confirm with Nexus/Raidcore**  
   - How addons are supposed to get ImGui (shared lib, version, build).  
   - Whether addons must use a specific ImGui version or package (e.g. lib_imgui) to avoid context/ABI mismatch.

3. **Rust example**  
   The nexus-rs example works because the Rust bindings and the loader share the same ImGui; the addon does not ship its own ImGui.

## Current workaround in this addon

- We **do not** draw any ImGui from the render callback (no launcher, no main window when opened by icon/keybind).
- We only register the Quick Access icon (texture + Add) and return; that path does not crash.
- The keybind callback is left empty so clicking the icon does nothing and does not trigger any draw.
- So the addon is “icon only” until we switch to the loader’s ImGui (or a shared lib_imgui) and can safely use `APIDefs->ImguiContext` and draw from the render callback.
