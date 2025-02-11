// 4004 emulator.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

int main() {
    setlocale(LC_ALL, "Russian");
    string filename = "prog.txt";
    cout << "Filename = " << filename << endl;
    // cin >> filename;

    ifstream file(filename);
    if (!file.is_open()) {
        cout << "File not found!" << endl;
        return 1;
    }

    vector<int> commands;
    vector<string> comments;

    string line;
    int number;
    string text;

    while (getline(file, line)) {
        stringstream ss(line);
        if (ss >> number) { // Проверяем, есть ли число в начале строки
            commands.push_back(number); // Добавляем число в массив
        }
        text = "";
        ss.ignore(1, ' '); // Пропускаем символ ' '
        ss.ignore(1, '#'); // Пропускаем символ '#'
        getline(ss, text); // Считываем текст после '#'
        comments.push_back(text); // Добавляем текст в массив
    }

    file.close();

    // Вывод результатов
    
    cout << "Код" << "\t" << "Коммент" << endl;
    for (int i = 0; i < commands.size(); ++i)
    {
        cout << commands.at(i) << "\t" << comments.at(i) << endl;
    }

    cout << endl << "Начинаем выполнение" << endl << endl;

    //основной цикл программы

    int program_counter = 0; //счетчик команд

    while (1)
    {
        //основной цикл
        //выводим текущую команду
        cout << commands.at(program_counter) << endl;
        //декодер комманд



    }


    return 0;
}

