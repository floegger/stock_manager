#ifndef TUI_H
#define TUI_H

#include <string>
#include <vector>

static constexpr int WIDTH = 70;  // inner content width (between the │ borders)

void top_border ();
void mid_border ();
void bot_border ();
void row ( const std::string &text );
void blank ();

std::string draw_screen ( const std::vector<std::string> &output_lines, const std::string &status );

std::string input_screen ( const std::vector<std::string> &output_lines, const std::string &status,
                           const std::string &label );

std::vector<std::string> wrap_lines ( const std::string &raw );

#endif  // TUI_H
