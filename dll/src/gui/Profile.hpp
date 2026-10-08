#pragma once

namespace gui::profile {

struct Stats {
    float listUs = 0.f;
    float detailsUs = 0.f;
    float frameUs = 0.f;
    int rows = 0;
    int cmds = 0;
    int vertices = 0;
    int windows = 0;
    double seen = -100.0;
};

const Stats& stats();
bool recording();
enum class Stage { Live, Core, Submit };
void sample(Stage stage, double us);
double stamp();
double since(double from);

void row();
void height(float h);
void probe(float y, float scroll);
void phase(const char* name);
void menu(float listUs, float detailsUs);
void frame(float frameUs);
void finish(float dt);

}
