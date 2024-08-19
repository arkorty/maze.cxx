#include <array>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sys/ioctl.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#include <vector>

class Entity {
  public:
    size_t xIdx;
    size_t yIdx;

    enum class Move {
        Up,
        Down,
        Left,
        Right,
    };

    Entity(size_t x, size_t y) : xIdx(x), yIdx(y) {}
};

class Maze {
  public:
    size_t rowSize;
    size_t colSize;
    std::unique_ptr<Entity> player;
    std::unique_ptr<Entity> start;
    std::unique_ptr<Entity> end;
    std::vector<std::vector<uint32_t>> pntr;

    Maze(size_t rowSize, size_t colSize) : rowSize(rowSize), colSize(colSize) {
        pntr.resize(colSize, std::vector<uint32_t>(rowSize, 0));
    }
};

class Game {
  private:
    struct termios term_state;
    size_t termColSize;
    size_t termRowSize;
    std::unique_ptr<Maze> maze;
    std::mutex mtx;
    std::condition_variable cv;
    bool updateNeeded;
    bool quitGame;
    bool winGame;

    static constexpr std::array<char, 5> sprites = {' ', 'H', '*', 'X', 'O'};

  public:
    Game() : updateNeeded(false), quitGame(false), winGame(false) {}

    void getTermSize() {
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
            throw std::runtime_error("Failed to get terminal size");
        }
        termRowSize = w.ws_row;
        termColSize = w.ws_col;
    }

    void termTooSmallHandler() const {
        if (termColSize < 2 * maze->rowSize || termRowSize < maze->colSize) {
        }
    }

    void getMazeSize(const std::string &path) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error("Couldn't find map file. Check file path.");
        }

        std::string line;
        size_t x_cnt = 0;
        size_t y_cnt = 0;
        if (std::getline(file, line)) {
            x_cnt = line.length();
            y_cnt = 1;
        }
        while (std::getline(file, line)) {
            ++y_cnt;
        }

        maze->rowSize = x_cnt;
        maze->colSize = y_cnt;
    }

    void move(Entity::Move move) {
        auto &player = maze->player;
        auto &pntr = maze->pntr;

        auto updatePosition = [&](int dx, int dy) {
            if (player->xIdx == maze->start->xIdx && player->yIdx == maze->start->yIdx) {
                pntr[player->yIdx][player->xIdx] = 2;
            } else {
                pntr[player->yIdx][player->xIdx] = 0;
            }
            player->xIdx += dx;
            player->yIdx += dy;
            pntr[player->yIdx][player->xIdx] = 4;
        };

        switch (move) {
        case Entity::Move::Left:
            if (player->xIdx != 0 && pntr[player->yIdx][player->xIdx - 1] != 1) {
                updatePosition(-1, 0);
            }
            break;
        case Entity::Move::Down:
            if (player->yIdx != maze->colSize - 1 && pntr[player->yIdx + 1][player->xIdx] != 1) {
                updatePosition(0, 1);
            }
            break;
        case Entity::Move::Up:
            if (player->yIdx != 0 && pntr[player->yIdx - 1][player->xIdx] != 1) {
                updatePosition(0, -1);
            }
            break;
        case Entity::Move::Right:
            if (player->xIdx != maze->rowSize - 1 && pntr[player->yIdx][player->xIdx + 1] != 1) {
                updatePosition(1, 0);
            }
            break;
        }
    }

    void restoreCursor() const { std::cout << "\033[u"; }

    void saveCursor() const { std::cout << "\033[s"; }

    void zeroCursor() { std::cout << "\033[0;0H" << std::flush; }

    void allocMazeBuffer(const std::string &path) {
        maze = std::make_unique<Maze>(0, 0);
        maze->player = std::make_unique<Entity>(0, 0);
        maze->start = std::make_unique<Entity>(0, 0);
        maze->end = std::make_unique<Entity>(0, 0);
        getMazeSize(path);

        maze->pntr.resize(maze->colSize, std::vector<uint32_t>(maze->rowSize, 0));
    }

    void loadMazeToBuffer(const std::string &path) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error("Couldn't find map file. Check file path.");
        }

        std::string line;
        size_t y_cord = 0;
        while (std::getline(file, line)) {
            for (size_t x_cord = 0; x_cord < line.length(); ++x_cord) {
                char cur = line[x_cord];
                if (cur == '4') {
                    maze->player->xIdx = x_cord;
                    maze->player->yIdx = y_cord;
                    maze->start->xIdx = x_cord;
                    maze->start->yIdx = y_cord;
                } else if (cur == '3') {
                    maze->end->xIdx = x_cord;
                    maze->end->yIdx = y_cord;
                }
                maze->pntr[y_cord][x_cord] = static_cast<uint32_t>(cur - '0');
            }
            ++y_cord;
        }
    }

    void print() const {
        saveCursor();
        for (size_t y_cord = 0; y_cord < maze->colSize; ++y_cord) {
            for (size_t x_cord = 0; x_cord < maze->rowSize; ++x_cord) {
                std::cout << sprites[maze->pntr[y_cord][x_cord]];
                if (x_cord < maze->rowSize - 1) {
                    std::cout << ' ';
                }
            }
            if (y_cord < maze->colSize - 1) {
                std::cout << '\n';
            }
        }
        restoreCursor();
    }

    void setTermDef() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_state); }

    void setTermRaw() {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &raw);
        tcgetattr(STDIN_FILENO, &term_state);

        raw.c_lflag &= ~(ECHO | ICANON);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    void clear() {
        zeroCursor();
        saveCursor();
        for (size_t y_cord = 0; y_cord < termRowSize; ++y_cord) {
            for (size_t x_cord = 0; x_cord < termColSize; ++x_cord) {
                std::cout << ' ';
            }
            if (y_cord < maze->colSize - 1) {
                std::cout << '\n';
            }
        }
        restoreCursor();
    }

    void checkWin() {
        if (maze->player->yIdx == maze->end->yIdx && maze->player->xIdx == maze->end->xIdx) {
            winGame = true;
        }
    }

    void capture() {
        setTermRaw();

        char ch;
        while ((ch = getchar()) != 'q') {
            switch (ch) {
            case 'w':
                move(Entity::Move::Up);
                break;
            case 'a':
                move(Entity::Move::Left);
                break;
            case 's':
                move(Entity::Move::Down);
                break;
            case 'd':
                move(Entity::Move::Right);
                break;
            default:
                continue;
            }

            checkWin();
            if (winGame) {
                break;
            }

            {
                std::lock_guard<std::mutex> lock(mtx);
                updateNeeded = true;
            }
            cv.notify_one();
        }

        quitGame = true;
        cv.notify_one();
    }

    void update() {
        zeroCursor();
        clear();
        print();

        while (!quitGame) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this] { return updateNeeded || quitGame; });

            if (quitGame)
                break;

            if (updateNeeded) {
                print();
                updateNeeded = false;
            }
        }

        if (winGame) {
            clear();
            zeroCursor();
            std::cout << "Congratulations! You have won the game.\n";
        } else {
            clear();
            zeroCursor();
            std::cout << "Keyboard interrupt! Quitting now...\n";
        }
    }

    void startGame(int argc, char *argv[]) {
        if (argc != 2) {
            throw std::runtime_error("Usage: " + std::string(argv[0]) + " <maze_file>");
        }

        try {
            getTermSize();
            allocMazeBuffer(argv[1]);
            termTooSmallHandler();
            loadMazeToBuffer(argv[1]);

            std::thread updateThread(&Game::update, this);
            std::thread captureThread(&Game::capture, this);

            updateThread.join();
            captureThread.join();
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << std::endl;
            exit(1);
        }
    }
};

int main(int argc, char *argv[]) {
    Game game;
    game.startGame(argc, argv);
    return 0;
}
