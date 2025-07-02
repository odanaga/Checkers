#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
  public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Height")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc); //лог файл статистики игры
        fout.close();
    }

    // to start checkers
    int play()
    {
        auto start = chrono::steady_clock::now();
        if (is_replay) //обработка значения рестарта
        {
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            board.start_draw();
        }
        is_replay = false;

        int turn_num = -1;
        bool is_quit = false;
        const int Max_turns = config("Game", "MaxNumTurns");
        while (++turn_num < Max_turns)
        {
            beat_series = 0;
            logic.find_turns(turn_num % 2); //находим доступные ходы
            if (logic.turns.empty())
                break;
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot"))) //определяем чей ход
            {
                auto resp = player_turn(turn_num % 2);
                if (resp == Response::QUIT) //обработка кнопки выхода
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY) //обработка кнопки рестарта
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK) //обработка кнопки отмены хода
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else //иначе вызова хода бота
                bot_turn(turn_num % 2);
        }
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        if (is_replay) //на рестарте вызываем функцию игру по новой
            return play();
        if (is_quit) //останавливаем программу
            return 0;
        int res = 2;
        if (turn_num == Max_turns)
        {
            res = 0;
        }
        else if (turn_num % 2)
        {
            res = 1;
        }
        board.show_final(res);
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }
        return res;
    }

  private:
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now();

        auto delay_ms = config("Bot", "BotDelayMS");
        // new thread for equal delay for each turn
        thread th(SDL_Delay, delay_ms); //задержка перед ходом бота
        auto turns = logic.find_best_turns(color); //находим лучшие ходы
        th.join();
        bool is_first = true;
        // making moves //выполняем ходы
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            beat_series += (turn.xb != -1);
            board.move_piece(turn, beat_series);
        }

        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)
    {
        // return 1 if quit
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells); //подсветка шашек с доступными ходами
        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;
        // trying to make first move
        while (true)
        {
            auto resp = hand.get_cell(); //получаем клик от игрока
            if (get<0>(resp) != Response::CELL) //если это не выбор клетки, то возвращаем ответ для дальнейшей обработки
                return get<0>(resp);
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            bool is_correct = false; //проверка правильности хода или выбора клетки
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second) //проверяем совпадает ли выбранная клетка с каким-нибудь возможным ходом или шашкой, имеющей ходы
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second}) //передвижение шашки виртуально
                {
                    pos = turn;
                    break;
                }
            }
            if (pos.x != -1) //если какой-то ход был сделан, то останавливаем общий цикл выбора хода
                break;
            if (!is_correct) //если нельзя выбрать, то сбрасываем подсветку клеток в нужное состояние и продолжаем общий цикл
            {
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);
            vector<pair<POS_T, POS_T>> cells2; //подсветка доступных ходов для выбранной шашки
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1); //передвижение шашки на доске
        if (pos.xb == -1) //если ничего не было побито, то заканчиваем ход
            return Response::OK;
        // continue beating while can
        beat_series = 1;
        while (true) //продолжаем серию
        {
            logic.find_turns(pos.x2, pos.y2);
            if (!logic.have_beats) //если нельзя продолжить, то заканчиваем ход
                break;

            vector<pair<POS_T, POS_T>> cells; //подсветка доступных ходов
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);
            // trying to make move
            while (true)
            {
                auto resp = hand.get_cell(); //получаем инпут от игрока так же, как и раньше
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                bool is_correct = false; //проверяем корректность
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break; //виртуально передвигаем шашку
                    }
                }
                if (!is_correct) //продолжение цикла, если игрок попытался сделать недоступный ход пока не будет сделан доступный ход
                    continue;

                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series); //двигаем шашку на доске и заканчиваем ход
                break;
            }
        }

        return Response::OK; //заканчиваем ход
    }

  private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
