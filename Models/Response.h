#pragma once

enum class Response
{
    OK, //окончание хода
    BACK, //откат хода
    REPLAY, //рестарт
    QUIT, //закрытие окна
    CELL //выбор клетки
};
