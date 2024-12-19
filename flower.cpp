#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

constexpr double PI = 3.14159265358979323846;

// Shading characters from darkest to lightest
const std::string SHADE_CHARS = "@%#*+=-:. ";

struct Point3D {
    double x, y, z;
};

struct FlowerParams {
    double centerX, centerY;
    double time;
    int numPetals;
    double baseRadius;
    double height;
    double curl;
    double waveSpeed;
};

// Get terminal size
struct winsize getTerminalSize() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w;
}

// Terminal handling functions
void setupTerminal() {
    struct termios term;
    tcgetattr(STDOUT_FILENO, &term);
    term.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDOUT_FILENO, TCSANOW, &term);
}

void resetTerminal() {
    struct termios term;
    tcgetattr(STDOUT_FILENO, &term);
    term.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDOUT_FILENO, TCSANOW, &term);
}

// 3D rotation matrices
Point3D rotateX(Point3D p, double angle) {
    double y = p.y * cos(angle) - p.z * sin(angle);
    double z = p.y * sin(angle) + p.z * cos(angle);
    return {p.x, y, z};
}

Point3D rotateY(Point3D p, double angle) {
    double x = p.x * cos(angle) + p.z * sin(angle);
    double z = -p.x * sin(angle) + p.z * cos(angle);
    return {x, p.y, z};
}

Point3D rotateZ(Point3D p, double angle) {
    double x = p.x * cos(angle) - p.y * sin(angle);
    double y = p.x * sin(angle) + p.y * cos(angle);
    return {x, y, p.z};
}

// Calculate shadow intensity based on normal vector and light direction
char getShadeChar(const Point3D& normal, const Point3D& lightDir) {
    // Normalize vectors
    double len = sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    Point3D n = {normal.x / len, normal.y / len, normal.z / len};
    
    len = sqrt(lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z);
    Point3D l = {lightDir.x / len, lightDir.y / len, lightDir.z / len};
    
    // Calculate dot product
    double dot = n.x * l.x + n.y * l.y + n.z * l.z;
    dot = std::max(0.0, std::min(1.0, dot));
    
    // Map to shading character
    int index = static_cast<int>(dot * (SHADE_CHARS.length() - 1));
    return SHADE_CHARS[index];
}

class FlowerRenderer {
private:
    struct winsize termSize;
    std::vector<std::vector<char>> buffer;
    std::vector<std::vector<double>> zBuffer;
    
public:
    FlowerRenderer() {
        termSize = getTerminalSize();
        buffer.resize(termSize.ws_row, std::vector<char>(termSize.ws_col, ' '));
        zBuffer.resize(termSize.ws_row, std::vector<double>(termSize.ws_col, std::numeric_limits<double>::infinity()));
    }
    
    void clear() {
        for (auto& row : buffer) {
            std::fill(row.begin(), row.end(), ' ');
        }
        for (auto& row : zBuffer) {
            std::fill(row.begin(), row.end(), std::numeric_limits<double>::infinity());
        }
    }
    
    void setPoint(int x, int y, double z, char c) {
        if (x >= 0 && x < termSize.ws_col && y >= 0 && y < termSize.ws_row) {
            if (z < zBuffer[y][x]) {
                buffer[y][x] = c;
                zBuffer[y][x] = z;
            }
        }
    }
    
    void render() {
        std::cout << "\033[H";  // Move cursor to home position
        for (const auto& row : buffer) {
            for (char c : row) {
                std::cout << c;
            }
            std::cout << '\n';
        }
    }
    
    void drawPetal(const FlowerParams& params, double angle, double phase) {
        double petalLength = params.baseRadius * (1.0 + 0.5 * sin(params.time * params.waveSpeed));
        double curl = params.curl * sin(params.time * params.waveSpeed + phase);
        
        // Generate petal points with curl effect
        for (double t = 0; t <= 1.0; t += 0.05) {
            double r = petalLength * t;
            double height = params.height * sin(PI * t) * (1.0 + 0.2 * sin(params.time * params.waveSpeed + phase));
            
            Point3D p = {
                r * cos(angle),
                r * sin(angle),
                height + curl * sin(PI * t)
            };
            
            // Apply rotations
            p = rotateZ(p, 0.3 * sin(params.time + phase));
            p = rotateY(p, 0.2 * sin(params.time * 0.7 + phase));
            
            // Project to screen space
            int screenX = static_cast<int>(params.centerX + p.x);
            int screenY = static_cast<int>(params.centerY + p.y * 0.5 - p.z * 0.5);
            
            // Calculate normal for shading
            Point3D normal = {
                cos(angle) * (1.0 - t),
                sin(angle) * (1.0 - t),
                0.5 + 0.5 * sin(PI * t)
            };
            
            // Light direction (adjustable)
            Point3D lightDir = {0.5, -0.5, 1.0};
            
            char shadeChar = getShadeChar(normal, lightDir);
            setPoint(screenX, screenY, p.z, shadeChar);
        }
    }
    
    void drawFlower(const FlowerParams& params) {
        clear();
        
        // Draw petals
        for (int i = 0; i < params.numPetals; i++) {
            double angle = (2.0 * PI * i) / params.numPetals;
            double phase = (2.0 * PI * i) / params.numPetals;
            drawPetal(params, angle, phase);
        }
        
        // Draw stem
        for (int i = 1; i <= 8; i++) {
            double wave = 0.5 * sin(params.time * 2.0 + i * 0.2);
            setPoint(
                static_cast<int>(params.centerX + wave),
                static_cast<int>(params.centerY + i),
                0,
                '|'
            );
        }
        
        render();
    }
};

int main() {
    setupTerminal();
    std::cout << "\033[2J";  // Clear screen
    
    struct winsize term = getTerminalSize();
    FlowerParams params {
        static_cast<double>(term.ws_col) / 2,  // centerX
        static_cast<double>(term.ws_row) / 3,  // centerY
        10.0,    // time
        10,      // numPetals
        30.0,   // baseRadius
        4.0,    // height
        10.0,    // curl
        1.5     // waveSpeed
    };
    
    FlowerRenderer renderer;
    
    try {
        while (true) {
            renderer.drawFlower(params);
            params.time += 0.1;
            
            // Check for 'q' key press
            char input;
            if (read(STDIN_FILENO, &input, 1) > 0 && input == 'q') {
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    } catch (...) {
        resetTerminal();
        throw;
    }
    
    resetTerminal();
    std::cout << "\033[2J\033[H";  // Clear screen and home cursor
    return 0;
}