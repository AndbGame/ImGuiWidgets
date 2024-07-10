#include "Renderer.h"

#include <d3d11.h>

#define IMGUI_DEFINE_MATH_OPERATORS

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <dxgi.h>

#include "imgui_internal.h"

namespace ImGuiWidgets
{
	LRESULT Renderer::WndProcHook::thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto& io = ImGui::GetIO();
		if (uMsg == WM_KILLFOCUS) {
			io.ClearInputCharacters();
			io.ClearInputKeys();
		}

		return func(hWnd, uMsg, wParam, lParam);
	}

	void Renderer::D3DInitHook::thunk()
	{
		func();

		INFO("D3DInit Hooked!");
		auto render_manager = RE::BSGraphics::Renderer::GetSingleton();
		if (!render_manager) {
			ERROR("Cannot find render manager. Initialization failed!");
			return;
		}

		INFO("Getting swapchain...");
		auto swapchain = render_manager->GetCurrentRenderWindow()->swapChain;
		if (!swapchain) {
			ERROR("Cannot find swapchain. Initialization failed!");
			return;
		}

		INFO("Getting swapchain desc...");
		REX::W32::DXGI_SWAP_CHAIN_DESC sd{};
		if (swapchain->GetDesc(std::addressof(sd)) < 0) {
			ERROR("IDXGISwapChain::GetDesc failed.");
			return;
		}

		auto render_data = render_manager->GetRuntimeData();

		device = render_data.forwarder;
		context = render_data.context;

		INFO("Initializing ImGui...");
		ImGui::CreateContext();
		if (!ImGui_ImplWin32_Init((void*)(sd.outputWindow))) {
			ERROR("ImGui initialization failed (Win32)");
			return;
		}
		if (!ImGui_ImplDX11_Init((ID3D11Device*)(device), (ID3D11DeviceContext*)(context))) {
			ERROR("ImGui initialization failed (DX11)");
			return;
		}
		INFO("ImGui initialized!");

		initialized.store(true);

		WndProcHook::func = reinterpret_cast<WNDPROC>(
			SetWindowLongPtrA(
				sd.outputWindow,
				GWLP_WNDPROC,
				reinterpret_cast<LONG_PTR>(WndProcHook::thunk)));
		if (!WndProcHook::func)
			ERROR("SetWindowLongPtrA failed!");
	}

	void Renderer::DXGIPresentHook::thunk(std::uint32_t a_p1)
	{
		func(a_p1);

		if (!D3DInitHook::initialized.load())
			return;

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

        ImGui::Begin("Hello, world!");  // Create a window called "Hello, world!" and append into it.

		ImGui::Text("This is some useful text.");           // Display some text (you can use a format strings too)

		if (ImGui::Button("Button")) {  // Buttons return true when clicked (most widgets return true when edited/activated)
		
		}
		ImGui::SameLine();
		ImGui::Text("counter = %d", 3);
		auto& io = ImGui::GetIO();
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::End();

		ImGui::EndFrame();
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	}

	void Renderer::MessageCallback(SKSE::MessagingInterface::Message* msg)  //CallBack & LoadTextureFromFile should called after resource loaded.
	{
		if (msg->type == SKSE::MessagingInterface::kDataLoaded && D3DInitHook::initialized) {
		}
	}

	bool Renderer::Install()
	{
		auto g_message = SKSE::GetMessagingInterface();
		if (!g_message) {
			ERROR("Messaging Interface Not Found!");
			return false;
		}

		g_message->RegisterListener(MessageCallback);

		stl::write_thunk_call<D3DInitHook>();
		stl::write_thunk_call<DXGIPresentHook>();

		return true;
	}
}