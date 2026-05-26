#pragma once

namespace USTC_CG
{
class Shape
{
   public:
    // Draw Settings
    struct Config
    {
        // Offset to convert canvas position to screen position
        float bias[2] = { 0.f, 0.f };
        // Line color in RGBA format
        unsigned char line_color[4] = { 255, 0, 0, 255 };
        float line_thickness = 2.0f;
    };
    
   protected:
    // Shape's own color (RGB)
    unsigned char color_[3] = { 255, 0, 0 }; // Default red color
    
    // Shape's own line thickness
    float line_thickness_ = 2.0f; // Default thickness

   public:
    // Default constructor
    Shape() {
        color_[0] = 255; // Red
        color_[1] = 0;   // Green
        color_[2] = 0;   // Blue
    }
    
    virtual ~Shape() = default;

    /**
     * Sets the shape's color.
     * @param r, g, b Color components in RGB format (0-255).
     */
    void set_color(unsigned char r, unsigned char g, unsigned char b)
    {
        color_[0] = r;
        color_[1] = g;
        color_[2] = b;
    }
    
    /**
     * Sets the shape's line thickness.
     * @param thickness Line thickness value.
     */
    void set_line_thickness(float thickness)
    {
        line_thickness_ = thickness;
    }
    
    /**
     * Draws the shape on the screen.
     * This is a pure virtual function that must be implemented by all derived
     * classes.
     *
     * @param config The configuration settings for drawing, including line
     * thickness, and bias.
     *               - line_thickness determines how thick the outline will be.
     *               - bias is used to adjust the shape's position on the
     * screen.
     */
    virtual void draw(const Config& config) const = 0;
    /**
     * Updates the state of the shape.
     * This function allows for dynamic modification of the shape, in response
     * to user interactions like dragging.
     *
     * @param x, y Dragging point. e.g. end point of a line.
     */
    virtual void update(float x, float y) = 0;
    /**
     * Adds a control point to the shape.
     * This function is used to add control points to the shape, which can be
     * used to modify the shape's appearance.
     *
     * @param x, y Control point to be added. e.g. vertex of a polygon.
     */
    virtual void add_control_point(float x, float y) {}
    
    /**
     * Gets the shape's color.
     * @return Pointer to the color array (RGB).
     */
    const unsigned char* get_color() const { return color_; }
    
    /**
     * Gets the shape's line thickness.
     * @return Line thickness value.
     */
    float get_line_thickness() const { return line_thickness_; }
};
}  // namespace USTC_CG