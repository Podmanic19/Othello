#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <random>

enum class Command;
enum class Square;
enum class Direction;

struct Game;

typedef std::pair<int, int> coord;

const char ASCII_1 = 49;
const char ASCII_A = 65;
const uint8_t LINE_LENGTH = 8;
const uint8_t NUM_SQUARES = 64;
const uint8_t MAX_DEPTH = 5;
const std::array<coord, 4> CORNERS = {coord{0, 0}, coord{0, 7}, coord{7, 0}, coord{7, 7}};
std::mutex mutex;

enum class Command
{
    start,
    stop,
    move,
    play
};

enum class Square
{
    empty,
    black,
    white
};

enum class Stability
{
    stable,
    indifferent,
    unstable,
    unassigned
};

struct Game
{

    bool started;
    int time;
    short black_squares;
    short white_squares;
    Square my_colour;
    std::array<std::array<Square, LINE_LENGTH>, LINE_LENGTH> board;

    Game()
    {

        started = false;
        time = 2;
        black_squares = 2;
        white_squares = 2;
        my_colour = Square::white;

        for (uint8_t i = 0; i < LINE_LENGTH; i++)
        {
            for (uint8_t j = 0; j < LINE_LENGTH; j++)
            {
                board[i][j] = Square::empty;
            }
        }

        board[3][3] = Square::white;
        board[4][4] = Square::white;

        board[3][4] = Square::black;
        board[4][3] = Square::black;
    }
};

struct Move
{

    double score = 0;
    coord coords;

    Move(int x, int y)
    {
        score = 0;
        coords.first = x;
        coords.second = y;
    }

    Move()
    {
        score = 0;
        coords.first = 2;
        coords.second = 3;
    }

    std::string to_string()
    {
        char first = ASCII_1 + (char)coords.first;
        char second = ASCII_A + (char)coords.second;
        return std::string() + second + first;
    }
};

Command get_command(std::string line)
{

    std::string command;
    std::stringstream ss(line);
    ss >> command;

    if (command.compare("START") == 0)
    {
        return Command::start;
    }
    if (command.compare("MOVE") == 0)
    {
        return Command::move;
    }
    if (command.compare("STOP") == 0)
    {
        return Command::stop;
    }
    if (command.compare("PLAY") == 0)
    {
        return Command::play;
    }

    std::clog << "Invalid command";
    exit(1);
}

Game get_params(const std::string &input)
{

    Game game = Game();
    std::stringstream ss(input);
    std::string colour;
    std::string time_str;
    int time;

    ss >> colour;
    ss >> colour;
    ss >> time;

    if (colour.compare("W") == 0 || colour.compare("B") == 0)
    {
        colour.compare("W") == 0 ? game.my_colour = Square::white : game.my_colour = Square::black;
    }
    else
    {
        std::clog << "Invalid Colour paramter";
        exit(1);
    }

    if (time < 1)
    {
        std::clog << "Invalid time paramter";
        exit(1);
    }

    game.time = time;
    game.started = true;

    return game;
}

void get_state(const std::string &input, Game &game)
{

    std::stringstream ss(input);
    std::string state;

    ss >> state;
    ss >> state;

    if (game.started == false)
    {
        std::clog << "MOVE called before START.";
        exit(1);
    }
    if (state.size() > NUM_SQUARES)
    {
        std::clog << "Invalid game state on input.";
        exit(1);
    }

    game.black_squares = 0;
    game.white_squares = 0;

    for (uint8_t i = 0; i < NUM_SQUARES; i++)
    {

        Square square;
        if (state[i] == '-')
            square = Square::empty;
        else if (state[i] == 'X')
        {
            game.black_squares++;
            square = Square::black;
        }
        else if (state[i] == 'O')
        {
            game.white_squares++;
            square = Square::white;
        }
        else
        {
            std::clog << "Invalid game state on input.";
            exit(1);
        }

        game.board[i / LINE_LENGTH][i % LINE_LENGTH] = square;
    }
}

bool exists(coord pos)
{
    return ((pos.first >= 0 && pos.first < LINE_LENGTH) && (pos.second >= 0 && pos.second < LINE_LENGTH));
}

Square op_colour(Square my_colour)
{
    return my_colour == Square::black ? Square::white : Square::black;
}

std::vector<coord> get_neighbours(coord position)
{
    std::vector<coord> neighbours;

    for (int8_t change_row = -1; change_row <= 1; change_row++)
    {
        for (int8_t change_col = -1; change_col <= 1; change_col++)
        { //checking surrounding squares
            if (change_row == 0 && change_col == 0)
                continue; //skip this square
            coord neighbour{position.first + change_row, position.second + change_col};
            if (!exists(neighbour))
            {
                continue;
            }
            neighbours.push_back(neighbour);
        }
    }

    return neighbours;
}

coord get_direction(coord position, coord neighbour)
{
    return {neighbour.first - position.first, neighbour.second - position.second};
}

bool search_for_square(const Game &current_state, Square searched, Square to_skip, coord position, coord direction)
{
    for (uint8_t i = 1; i < LINE_LENGTH; i++)
    {
        coord neighbour{position.first + i * direction.first, position.second + i * direction.second};
        if (!exists(neighbour))
            return false;
        else if (current_state.board[neighbour.first][neighbour.second] == to_skip)
            continue;
        else if (current_state.board[neighbour.first][neighbour.second] == searched)
            return true;
        else
            return false;
    }
    return false;
}

void flip(Game &game, coord position, int8_t change_row, int8_t change_col, uint8_t iters)
{
    for (uint8_t i = 1; i <= iters; i++)
    {
        Square &current = game.board[position.first + i * change_row][position.second + i * change_col];
        if (current == Square::black)
        {
            game.black_squares--;
            game.white_squares++;
        }
        else
        {
            game.white_squares--;
            game.black_squares++;
        }
        current = op_colour(current);
    }
}

Game perform_move(const Game &game, Move move, Square colour)
{
    Game next_state{game};

    next_state.board[move.coords.first][move.coords.second] = colour;
    colour == Square::black ? next_state.black_squares++ : next_state.white_squares++;

    for (int8_t change_row = -1; change_row <= 1; change_row++)
    {
        for (int8_t change_col = -1; change_col <= 1; change_col++)
        { // checking surrounding squares
            bool flank_found = false;
            if (change_row == 0 && change_col == 0)
            {
                continue; // skip this square
            }
            uint8_t iters = 0;
            for (uint8_t i = 1; i < LINE_LENGTH; i++)
            {
                coord neighbour{move.coords.first + i * change_row, move.coords.second + i * change_col};
                if (!exists(neighbour) || game.board[neighbour.first][neighbour.second] == Square::empty)
                    break;
                if (game.board[neighbour.first][neighbour.second] == colour)
                {
                    flank_found = true;
                    break;
                }
                iters++; // number of squares to flip
            }
            if (iters > 0 && flank_found)
                flip(next_state, move.coords, change_row, change_col, iters);
        }
    }

    return next_state;
}

std::vector<Move> get_moves(const Game &game, Square colour)
{
    std::vector<Move> moves;

    for (uint8_t i = 0; i < LINE_LENGTH; i++)
    {
        for (uint8_t j = 0; j < LINE_LENGTH; j++)
        {
            coord pos{i, j};
            bool mine = false;
            bool found_flank = false;
            if (game.board[i][j] != Square::empty)
            {
                continue;
            }
            auto neighbours = get_neighbours(pos);
            for (auto neighbour : neighbours)
            {
                if (game.board[neighbour.first][neighbour.second] == Square::empty)
                    continue;
                mine = game.board[neighbour.first][neighbour.second] == op_colour(colour); //if neighbour is different colour
                found_flank = search_for_square(game, colour, op_colour(colour), pos, get_direction(pos, neighbour));
                if (mine && found_flank)
                {
                    moves.push_back(Move(i, j));
                }
            }
        }
    }
    return moves;
}

double corner_score(const Game &game)
{
    uint8_t mine_captured = 0;
    uint8_t opp_captured = 0;
    uint8_t mine_potential = 0;
    uint8_t opp_potential = 0;

    for (auto corner : CORNERS)
    {
        if (game.board[corner.first][corner.second] != Square::empty)
        {
            game.board[corner.first][corner.second] == game.my_colour ? mine_captured++ : opp_captured++;
            continue;
        }
        for (auto neighbour : get_neighbours(corner))
        {
            bool mine = false;
            bool added_mine = false;
            bool added_opp = false;
            mine = game.board[neighbour.first][neighbour.second] == op_colour(game.my_colour); //if neighbour is different colour
            if (mine && !added_mine)
            {
                added_mine = true;
                mine_potential++;
            }
            if (!mine && !added_opp)
            {
                added_opp = true;
                opp_potential++;
            }
        }
    }

    if ((mine_captured + mine_potential) + (opp_captured + opp_potential) == 0)
        return 0;

    return 100 * (mine_captured + mine_potential) / ((mine_captured + mine_potential) + (opp_captured + opp_potential));
}

double coin_score(const Game &game)
{
    short my_coins;
    short opp_coins;

    my_coins = game.my_colour == Square::black ? game.black_squares : game.white_squares;
    opp_coins = game.white_squares + game.black_squares - my_coins;

    return 100 * (my_coins - opp_coins) / (my_coins + opp_coins);
}

bool check_L_stability(std::vector<Stability> neighbours)
{
    uint8_t stable_in_line = 0;
    std::vector<uint8_t> row_1{0, 1, 2};
    std::vector<uint8_t> row_2{5, 6, 7};
    std::vector<uint8_t> col_1{0, 3, 5};
    std::vector<uint8_t> col_2{2, 4, 7};

    for (uint8_t i : row_1)
    {
        if (neighbours[i] == Stability::stable)
            stable_in_line++;
        else
            break;
    }
    if (stable_in_line == 3)
    {
        if (neighbours[3] == Stability::stable || neighbours[4] == Stability::stable)
            return true;
    }
    stable_in_line = 0;

    for (uint8_t i : row_2)
    {
        if (neighbours[i] == Stability::stable)
            stable_in_line++;
        else
            break;
    }
    if (stable_in_line == 3)
    {
        if (neighbours[3] == Stability::stable || neighbours[4] == Stability::stable)
            return true;
    }
    stable_in_line = 0;

    for (uint8_t i : col_1)
    {
        if (neighbours[i] == Stability::stable)
            stable_in_line++;
        else
            break;
    }
    if (stable_in_line == 3)
    {
        if (neighbours[1] == Stability::stable || neighbours[6] == Stability::stable)
            return true;
    }
    stable_in_line = 0;

    for (uint8_t i : col_2)
    {
        if (neighbours[i] == Stability::stable)
            stable_in_line++;
        else
            break;
    }
    if (stable_in_line == 3)
    {
        if (neighbours[1] == Stability::stable || neighbours[6] == Stability::stable)
            return true;
    }

    return false;
}

int stability_for_colour(const Game &game, Square colour)
{
    int8_t my_stability = 0;
    std::array<std::array<Stability, LINE_LENGTH>, LINE_LENGTH> stability_board;

    for (uint8_t i = 1; i < LINE_LENGTH; i++)
    {
        for (uint8_t j = 1; j < LINE_LENGTH; j++)
        {
            stability_board[i][j] = Stability::unassigned;
        }
    }

    for (auto corner : CORNERS)
    {
        if (game.board[corner.first][corner.second] == Square::empty)
            continue;
        for (int8_t change_row = -1; change_row <= 1; change_row++)
        {
            for (int8_t change_col = -1; change_col <= 1; change_col++)
            { //checking surrounding squares
                if ((change_row == 0 && change_col == 0) || (change_row != 0 && change_col != 0))
                    continue; //check columns and rows only
                for (uint8_t i = 0; i < LINE_LENGTH; i++)
                {
                    coord neighbour{corner.first + change_row, corner.second + change_col};
                    if (!exists(neighbour))
                        break;
                    if (game.board[neighbour.first][neighbour.second] == colour)
                    {
                        stability_board[neighbour.first][neighbour.second] = Stability::stable;
                        continue;
                    }
                    break;
                }
            }
        }
    }

    for (uint8_t i = 0; i < LINE_LENGTH; i++)
    {
        for (uint8_t j = 0; j < LINE_LENGTH; j++)
        {
            if (game.board[i][j] != colour || stability_board[i][j] != Stability::unassigned)
                continue; //skip squares that are not mine or are already assigned stability
            std::vector<Stability> neighbours;
            [&]
            {
                for (int8_t change_row = -1; change_row <= 1; change_row++)
                {
                    for (int8_t change_col = -1; change_col <= 1; change_col++)
                    { //checking surrounding squares
                        if (change_row == 0 && change_col == 0)
                            continue;
                        coord neighbour{i + change_row, j + change_col};
                        if (!exists(neighbour))
                        {
                            neighbours.push_back(Stability::stable);
                            continue;
                        }
                        if (game.board[neighbour.first][neighbour.second] == op_colour(colour))
                        { //if bordering opponents square
                            if (search_for_square(game, Square::empty, colour, neighbour, std::make_pair(-change_row, -change_col)))
                            {
                                stability_board[i][j] = Stability::unstable;
                                return;
                            }
                            else
                                stability_board[i][j] = Stability::indifferent;
                        }
                        if (game.board[neighbour.first][neighbour.second] == op_colour(colour))
                        { //if bordering empty square
                            if (search_for_square(game, op_colour(colour), colour, neighbour, std::make_pair(change_row, change_col)))
                            { //check if this is a possible move for opponent
                                stability_board[i][j] = Stability::unstable;
                                return;
                            }
                            else
                                stability_board[i][j] = Stability::indifferent;
                        }
                        neighbours.push_back(stability_board[neighbour.first][neighbour.second]);
                    }
                }
                if (check_L_stability(neighbours))
                {
                    stability_board[i][j] = Stability::stable;
                }
                else
                {
                    stability_board[i][j] = Stability::indifferent;
                }
            }();
            stability_board[i][j] == Stability::unstable ? my_stability-- : my_stability++;
        }
    }

    return my_stability;
}

double stability_score(const Game &game)
{
    int my_stability = stability_for_colour(game, game.my_colour);
    int op_stability = stability_for_colour(game, op_colour(game.my_colour));

    return my_stability + op_stability != 0 ? 100 * (my_stability - op_stability) / (my_stability + op_stability) : 0;
}

double mobility_score(const Game &game, const std::vector<Move> &my_moves)
{
    auto op_moves = get_moves(game, op_colour(game.my_colour));

    double my_moves_num, op_moves_num;

    my_moves_num = static_cast<double>(my_moves.size());
    op_moves_num = static_cast<double>(op_moves.size());

    return my_moves_num + op_moves_num != 0 ? 100 * (my_moves_num - op_moves_num) / (my_moves_num + op_moves_num) : 0;
}

double evaluate_state(const Game &current_state, const std::vector<Move> &moves)
{
    double score = 0;

    score += 20 * coin_score(current_state);
    score += 5 * mobility_score(current_state, moves);
    score += stability_score(current_state);
    score += 40 * corner_score(current_state);

    return score;
}

double minmax(const Game &current_state, bool &time_up, uint8_t depth, double alpha, double beta, Square colour)
{
    std::unique_lock<std::mutex> guard(mutex);
    if (time_up)
        return -1;
    guard.unlock();

    auto moves = get_moves(current_state, colour);

    if (depth == 0 || moves.empty())
    {
        return evaluate_state(current_state, moves);
    }

    bool my_colour = colour == current_state.my_colour;
    double value = std::numeric_limits<double>::infinity();
    value *= my_colour ? -1 : 1;

    for (Move m : moves)
    {
        auto next_state = perform_move(current_state, m, colour);
        value = std::max(value, minmax(next_state, time_up, depth - 1, alpha, beta, op_colour(colour)));
        if ((my_colour && value >= beta) || (!my_colour && value <= alpha))
        {
            break;
        }
        my_colour ? alpha = std::max(alpha, value) : beta = std::min(beta, value);
    }

    return value;
}

void play(const Game &current_state, Move &best_move, bool &time_up)
{
    auto moves = get_moves(current_state, current_state.my_colour);

    for (Move m : moves)
    {
        auto next_state = perform_move(current_state, m, current_state.my_colour);
        double inf = -std::numeric_limits<double>::infinity();
        m.score = minmax(next_state, time_up, MAX_DEPTH, -inf, inf, current_state.my_colour);
        std::lock_guard<std::mutex> guard(mutex);
        if (time_up)
        {
            return;
        }
        best_move = best_move.score < m.score ? m : best_move;
    }

    return;
}

void print_best_move(int time, bool &time_up, Move &best_move)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(time * 1000 - 750));
    std::lock_guard<std::mutex> guard(mutex);
    time_up = true;
    std::cout << best_move.to_string() << std::endl;
    return;
}

void print_state(const Game &game)
{
    for (uint8_t i = 0; i < LINE_LENGTH; i++)
    {
        if (i == 0)
            std::cout << "  A B C D E F G H" << std::endl;
        for (uint8_t j = 0; j < LINE_LENGTH; j++)
        {
            if (j == 0)
                std::cout << i + 1;
            char c = 'k';
            if (game.board[i][j] == Square::white)
                c = 'W';
            if (game.board[i][j] == Square::black)
                c = 'B';
            if (game.board[i][j] == Square::empty)
                c = '-';

            std::cout << "|" << c;
        }
        std::cout << "| \n";
    }
}

Move load_move(std::string move)
{
    Move m;
    std::stringstream ss;

    m.coords.second = move[0] - ASCII_A;
    m.coords.first = move[1] - ASCII_1;

    return m;
}

void start_command(std::string input, Game &game)
{
    game = get_params(input);
    std::cout << "1" << std::endl;
}

void move_command(std::string input, Game game)
{
    bool time_up = false;
    Move best_move{};

    get_state(input, game);
    if (get_moves(game, game.my_colour).empty())
    {
        exit(1);
    }
    std::thread t{play, std::ref(game), std::ref(best_move), std::ref(time_up)};
    print_best_move(game.time, time_up, best_move);
    if (t.joinable())
    {
        t.join();
    }
}

int main()
{
    std::string input;
    Game game;

    while (std::getline(std::cin, input))
    {

        std::string result;
        Command command;
        command = get_command(input);
        Move move;

        switch (command)
        {
        case Command::start:
            start_command(input, game);
            break;
        case Command::move:
            move_command(input, game);
            break;
        case Command::stop:
            return 0;
        }

        return 0;
    }
}