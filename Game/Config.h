#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

class Config
{
  public:
    Config()
    {
        reload();
    }

    void reload() //функция подгрузки настроек
    {
        std::ifstream fin(project_path + "settings1.json"); //открытие файла
        fin >> config; //передача данных из файла
        fin.close(); //закрытие файла
    }

    //данный оператор реализует получение значения настроек, первое значение раздел настроек, второе название настройки
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];
    }

  private:
    json config;
};
