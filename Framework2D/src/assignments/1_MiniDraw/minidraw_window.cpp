#include "minidraw_window.h"

#include <imgui.h>
#include <iostream>

namespace USTC_CG
{
MiniDraw::MiniDraw(const std::string& window_name) : Window(window_name)
{
    p_canvas_ = std::make_shared<Canvas>("Widget.Canvas");
}

MiniDraw::~MiniDraw()
{
    
}

void MiniDraw::draw()
{
    draw_canvas();
}

void MiniDraw::draw_canvas()
{
    // Set a full screen canvas view
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    if (ImGui::Begin(
            "Canvas",
            &flag_show_canvas_view_,
            ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground))
    {
        // Buttons for shape types
        if (ImGui::Button("Line"))
        {
            std::cout << "Set shape to Line" << std::endl;
            p_canvas_->set_line();
        }
        ImGui::SameLine();
        if (ImGui::Button("Rect"))
        {
            std::cout << "Set shape to Rect" << std::endl;
            p_canvas_->set_rect();
        }
        ImGui::SameLine();//水平排列按钮
        if (ImGui::Button("Ellipse"))
        {
            std::cout << "Set shape to Ellipse" << std::endl;
            p_canvas_->set_ellipse();
        }
        ImGui::SameLine();
        if(ImGui::Button("Polygon"))
        {
            std::cout<<"Set shape to Polygon" << std::endl;
            p_canvas_->set_polygon();
        }
        ImGui::SameLine();
        if(ImGui::Button("Freehand"))
        {
            std::cout<<"Set shape to Freehand" << std::endl;
            p_canvas_->set_freehand();
        }
        ImGui::SameLine();
        if(ImGui::Button("Freehand Smooth"))
        {
            std::cout<<"Set shape to Freehand Smooth" << std::endl;
            p_canvas_->set_freehand_smooth();
        }
        ImGui::SameLine();
        if(ImGui::Button("Save Canvas"))
        {
            std::cout<<"Save Canvas" << std::endl;
            p_canvas_->save_canvas("minidraw.png");
        }

        // HW1_TODO: More primitives
        //    - Ellipse
        //    - Polygon
        //    - Freehand(optional)
        
        // Color selection UI
        ImGui::Separator();
        ImGui::Text("Color Selection:");
        
        // Get current color values
        unsigned char r = p_canvas_->get_color_r();
        unsigned char g = p_canvas_->get_color_g();
        unsigned char b = p_canvas_->get_color_b();
        
        // RGB sliders
        bool color_changed = false;
        static int min_val = 0;
        static int max_val = 255;
        color_changed |= ImGui::SliderScalar("Red", ImGuiDataType_U8, &r, &min_val, &max_val, "%d");
        color_changed |= ImGui::SliderScalar("Green", ImGuiDataType_U8, &g, &min_val, &max_val, "%d");
        color_changed |= ImGui::SliderScalar("Blue", ImGuiDataType_U8, &b, &min_val, &max_val, "%d");
        
        // Update color if changed
        if (color_changed) {
            p_canvas_->set_color(r, g, b);
        }
        
        // Color preview box
        ImGui::SameLine();
        ImVec2 preview_pos = ImGui::GetCursorScreenPos();
        ImVec2 preview_size = ImVec2(50, 50);
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(preview_pos, ImVec2(preview_pos.x + preview_size.x, preview_pos.y + preview_size.y), IM_COL32(r, g, b, 255));
        draw_list->AddRect(preview_pos, ImVec2(preview_pos.x + preview_size.x, preview_pos.y + preview_size.y), IM_COL32(255, 255, 255, 255));
        ImGui::Dummy(preview_size);
        
        // Line thickness slider
        ImGui::Separator();
        ImGui::Text("Line Thickness:");
        float thickness = p_canvas_->get_line_thickness();
        if (ImGui::SliderFloat("Thickness", &thickness, 0.5f, 10.0f, "%.1f")) {
            p_canvas_->set_line_thickness(thickness);
        }
        
        // Canvas component
        ImGui::Text("Press left mouse to add shapes.");
        // Set the canvas to fill the rest of the window
        const auto& canvas_min = ImGui::GetCursorScreenPos();
        const auto& canvas_size = ImGui::GetContentRegionAvail();
        p_canvas_->set_attributes(canvas_min, canvas_size);
        p_canvas_->draw();
    }
    ImGui::End();
}
}  // namespace USTC_CG