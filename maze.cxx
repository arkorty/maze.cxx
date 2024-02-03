#include <chrono>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

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
    bool quitGame;
    bool winGame;

  public:
    Game() : quitGame(false), winGame(false) {}

    void getTermSize() {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        termRowSize = w.ws_row;
        termColSize = w.ws_col;
    }

    void termTooSmallHandler() {
        if (termColSize < 2 * maze->rowSize || termRowSize < maze->colSize) {
            std::cout << "Terminal is too small to display the whole maze.\n"
                         "Either make the terminal window bigger, or use a smaller maze map.\n";
            exit(0);
        }
    }

    void getMazeSize(const char *path) {
        FILE *file;
        if (!(file = fopen(path, "r"))) {
            std::cout << "Couldn't find map file. Check file path.\n";
            exit(0);
        }

        char ch;
        size_t x_cnt = 0;
        size_t y_cnt = 0;
        bool cnt_x = true;
        while ((ch = getc(file)) != EOF) {
            if (ch == '\n') {
                ++y_cnt;
                cnt_x = false;
            } else if (cnt_x) {
                ++x_cnt;
            }
        }

        maze->rowSize = x_cnt;
        maze->colSize = y_cnt;
    }

    void restoreCursor() { std::cout << "\033[u"; }

    void saveCursor() { std::cout << "\033[s"; }

    void zeroCursor() { std::cout << "\033[0;0H" << std::flush; }

    void move(Entity::Move move) {
        if (move == Entity::Move::Left && maze->player->xIdx != 0 &&
            maze->pntr[maze->player->yIdx][maze->player->xIdx - 1] != 1) {
            if (maze->player->xIdx == maze->start->xIdx && maze->player->yIdx == maze->start->yIdx) {
                maze->pntr[maze->player->yIdx][maze->player->xIdx] = 2;
            } else {
                maze->pntr[maze->player->yIdx][maze->player->xIdx] = 0;
            }
            --maze->player->xIdx;
            maze->pntr[maze->player->yIdx][maze->player->xIdx] = 4;
        }
        if (move == Entity::Move::Down && maze->player->yIdx != maze->colSize - 1 &&
            maze->pntr[maze->player->yIdx + 1][maze->player->xIdx] != 1) {
            if (maze->player->xIdx == maze->start->xIdx && maze->player->yIdx == maze->start->yIdx) {
                maze->pntr[maze->player->yIdx][maze->player->xIdx] = 2;
            } else {
                maze->pntr[maze->player->yIdx][maze->player->xIdx] = 0;
            }
            ++maze->player->yIdx;
            maze->pntr[maze->player->yIdx][maze->player->xIdx] = 4;
        }
        if (move == Entity::Move::Up && maze->player->yIdx != 0 &&
            maze->pntr[maze->player->yIdx - 1][maze->player->xIdx] != 1) {
            if (maze->player->xIdx == maze->start->xIdx && maze->player->yIdx == maze->start->yIdx) {
                maze->pntr[maze->player->yIdx][maze->player->xIdx] = 2;
            } else {
                maze->pntr[maze->player->yIdx][maze->player->xIdx] = 0;
            }
            --maze->player->yIdx;
            maze->pntr[maze->player->yIdx][maze->player->xIdx] = 4;
        }
        if (move == Entity::Move::Right && maze->player->xIdx != maze->rowSize - 1 &&
            maze->pntr[maze->player->yIdx][maze->player->xIdx + 1] != 1) {
            if (maze->player->xIdx == maze->start->xIdx && maze->player->yIdx == maze->start->yIdx) {
                maze->pntr[maze->player->yIdx][maze->player->xIdx] = 2;
            } else {
                maze->pntr[maze->player->yIdx][maze->player->xIdx] = 0;
            }
            ++maze->player->xIdx;
            maze->pntr[maze->player->yIdx][maze->player->xIdx] = 4;
        }
    }

    void freeMap() {
        // Free memory for maze and entities
        maze.reset();
    }

    void allocMazeBuffer(const char *path) {
        maze = std::make_unique<Maze>(0, 0);
        maze->player = std::make_unique<Entity>(0, 0);
        maze->start = std::make_unique<Entity>(0, 0);
        maze->end = std::make_unique<Entity>(0, 0);
        getMazeSize(path);

        maze->pntr.resize(maze->colSize, std::vector<uint32_t>(maze->rowSize, 0));
    }

    void loadMazeToBuffer(const char *path) {
        FILE *file;
        if (!(file = fopen(path, "r"))) {
            std::cout << "Couldn't find map file. Check file path.\n";
            exit(0);
        }

        size_t y_cord = 0;
        size_t x_cord = 0;

        char cur;
        while (EOF != (cur = getc(file))) {
            if ('\n' == cur) {
                x_cord = 0;
                ++y_cord;
                continue;
            } else if ('4' == cur) {
                maze->player->xIdx = x_cord;
                maze->player->yIdx = y_cord;
                maze->start->xIdx = x_cord;
                maze->start->yIdx = y_cord;
            } else if ('3' == cur) {
                maze->end->xIdx = x_cord;
                maze->end->yIdx = y_cord;
            }
            maze->pntr[y_cord][x_cord] = (uint32_t)(cur - '0');
            ++x_cord;
        }
    }

    void print() {
        const char sprites[] = " H*XO";
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
        ////    atexit(&Game::setTermDef);

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
            }

            checkWin();
            if (winGame) {
                break;
            }
        }

        quitGame = true;
    }

    void update() {
        zeroCursor();
        clear();
        while (!quitGame) {
            print();
            std::this_thread::sleep_for(std::chrono::microseconds(41667));
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
        if (argc > 1 && argc < 3) {
            getTermSize();
            allocMazeBuffer(argv[1]);
            termTooSmallHandler();
            loadMazeToBuffer(argv[1]);

            std::thread updateThread(&Game::update, this);
            std::thread captureThread(&Game::capture, this);

            updateThread.join();
            captureThread.join();
        }
    }
};

int main(int argc, char *argv[]) {
    Game game;
    game.startGame(argc, argv);
    return 0;
}
