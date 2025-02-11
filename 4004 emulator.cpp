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
        if (ss >> number) { // ���������, ���� �� ����� � ������ ������
            commands.push_back(number); // ��������� ����� � ������
        }
        text = "";
        ss.ignore(1, ' '); // ���������� ������ ' '
        ss.ignore(1, '#'); // ���������� ������ '#'
        getline(ss, text); // ��������� ����� ����� '#'
        comments.push_back(text); // ��������� ����� � ������
    }

    file.close();

    // ����� �����������
    
    cout << "���" << "\t" << "�������" << endl;
    for (int i = 0; i < commands.size(); ++i)
    {
        cout << commands.at(i) << "\t" << comments.at(i) << endl;
    }

    cout << endl << "�������� ����������" << endl << endl;

    //�������� ���� ���������

    int program_counter = 0; //������� ������

    while (1)
    {
        //�������� ����
        //������� ������� �������
        cout << commands.at(program_counter) << endl;
        //������� �������



    }


    return 0;
}

