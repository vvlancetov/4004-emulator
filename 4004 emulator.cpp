//Program file format: <command in decimal> # <comment>
//example
//  
//64 # jump to start
//0
//209 # ld 1
//



#include <Windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <chrono>
#include <thread>
#include <vector>
#include <conio.h>
#include <bitset>
#include <cmath>

using namespace std;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

struct ram_register
{
    vector<int> cell;
    vector<int> stat_char;
};

class Epson
{
    private:
        int ink_color = 0;    // 0 - black, 1 - red
        int curr_sector = 0;  //текущий сектор на барабане
        vector<string> paper; // виртуальная "бумага" для вывода информации
        unsigned int shifter; // сдвиговый регистр на 20 ячеек
        unsigned int sync_counter; //счетчик синхроимпульсов
        vector<string> drum; // виртуальный барабан принтера

    public:
        Epson()   // конструктор класса
            {
              shifter = 0;
              sync_counter = 0;
              
              // вставка "бумаги" в массив строк
              paper.push_back("                  "); 

              //инициализация барабана
              drum.push_back("000000000000000 d#");
              drum.push_back("111111111111111 +*");
              drum.push_back("222222222222222 -|");
              drum.push_back("333333333333333 x|");
              drum.push_back("444444444444444 /|");
              drum.push_back("555555555555555 MM");
              drum.push_back("666666666666666 MM");
              drum.push_back("777777777777777 ^T");
              drum.push_back("888888888888888 =K");
              drum.push_back("999999999999999 vE");
              drum.push_back("............... %E");
              drum.push_back("............... cC");
              drum.push_back("_______________ RM");
            }; 

        void put_data(int d);    //получение данных от процессора через порт RAM 0
        void shifter_in(int d);  //получение данных сдвиговым регистром
        int get_data();          //отправка данных процессору через порт ROM 2
        void sync();             //импульс синхронизации
        string get_shifter();    //печать содержимого регистра для отладки
};

//создаем принтер
Epson printer;

class Genius
{
    private:
        unsigned int shifter; // сдвиговый регистр на 10 ячеек
        bool key_pressed;     // индикатор нажатия клавиши
        int shifter_pattern; // маска для сравнения
        int data_for_port;   // выходной поток байтов

    public:
        Genius()  // конструктор класса
            {
            shifter = 0;
            };

        int get_data();          //отправка данных процессору через порт ROM 1
        string get_shifter();    //печать содержимого регистра для отладки
        void shifter_in(int d);  //получение данных сдвиговым регистром
        void press_key(int key); //нажатие клавиши на реальной клавиатуре
};

// создаем клавиатуру
Genius keyboard;

vector<int> commands;
vector<string> comments;
string filename = "BusiCom.txt"; //имя входного файла

//регистры процессора
int ACC; //Accumulator
bool CF; //Carry flag
bool TEST; //test signal
int RC; //Register Control
vector<int> stack;//стек команд
vector<int> registers;//внутренние регистры
vector<ram_register> ram;// RAM
vector<int>rom_ports; //порты ROM
vector<int>ram_ports; //порты RAM

int program_counter; //счетчик команд

// флаги для изменения работы
bool step_mode = false; //ждать ли нажатия пробела для выполнения команд
bool go_forward; //переменная для выхода из цикла обработки нажатий
bool RU_lang = false; //локализация
bool list_at_start = false; //вывод листинга на старте
bool log_to_console = false; //логирование команд на консоль


void print_all();
void list();

int main(int argc, char* argv[]) {

    //проверяем аргументы командной строки
    
    cout << "Checking command string parameters..." << endl;
    
    for (int i = 1; i < argc; i++)
    {
        string s = argv[i];
        if (s.substr(0, 2) == "-f")
        {
            //найдено имя файла
            if (argc >= i + 2)
            {
                filename = argv[i + 1];// "Prog.txt";
                cout << "new filename = " << filename << endl;
            }
            else
            {
                filename = "Prog.txt";
                cout << "filename = " << filename << " (default)"  << endl;
            }
        }
        if (s.substr(0, 3) == "-ru")
        {
            //использование русского языка
            setlocale(LC_ALL, "Russian");
            RU_lang = true;
            cout << "set RU lang" << endl;
        }
        if (s.substr(0, 5) == "-list")
        {
            //листинг в начале
            list_at_start = true;
            cout << "set listing after loadin ON " << endl;
        }
        if (s.substr(0, 5) == "-step")
        {
            //пошаговое выполнение
            step_mode = true;
            cout << "set step mode ON" << endl;
        }
        if (s.substr(0, 4) == "-log")
        {
            //пошаговое выполнение
            log_to_console = true;
            cout << "set log to console ON" << endl;
        }
    }

    if (!log_to_console)
    {
        step_mode = false; //отключаем пошаговый режим, если не задан вывод логов на экран
        cout << "set step mode ";
        SetConsoleTextAttribute(hConsole, 12);
        cout << "OFF";
        SetConsoleTextAttribute(hConsole, 7);
        cout << " because log to console is OFF" << endl;
    }

    ifstream file(filename);
    if (!file.is_open()) {
        if (RU_lang)
        {
            cout << "Файл ";
            SetConsoleTextAttribute(hConsole, 4);
            cout << filename;
            SetConsoleTextAttribute(hConsole, 7);
            cout << " не найден!" << endl;
        }
        else
        {
            cout << "File " << filename << " not found!" << endl;
        }
        return 1;
    }

    string line;
    int number;
    string text;
    
    //считываем данные из файла
    while (getline(file, line)) {
        stringstream ss(line);
        if (ss >> number) { // Проверяем, есть ли число в начале строки
            commands.push_back(number); // Добавляем число в массив
        }
        text = "";
        ss.ignore(1, ' '); // Пропускаем символ ' '
        ss.ignore(1, '#'); // Пропускаем символ '#'
        ss.ignore(1, ' '); // Пропускаем символ ' '
        getline(ss, text); // Считываем текст после '#'
        comments.push_back(text); // Добавляем текст в массив
    }
    file.close();

    //файл загружен
    if (RU_lang)
    {
        cout << "Загружено " << commands.size() << " команд(ы) из файла" << endl;
    }
    else
    {
        cout << "Loaded " << commands.size() << " commands from file" << endl;
    }

    //листинг
    if (list_at_start)
    {
        list(); // Вывод листинга программы
    }

    if (RU_lang)
    {
        SetConsoleTextAttribute(hConsole, 10);
        cout << "Начинаем выполнение... нажмите любую клавишу" << endl << endl;
        SetConsoleTextAttribute(hConsole, 7);
    }
    else
    {
        cout << endl << "Starting programm... ";
        SetConsoleTextAttribute(hConsole, 10);
        cout << "press a key" << endl << endl;
        SetConsoleTextAttribute(hConsole, 7);
    }

    //ждем нажатия клавиши
    while (!_kbhit()) std::this_thread::sleep_for(std::chrono::milliseconds(1));

    //выводим памятку
    if (step_mode)
    {
        if (RU_lang)
        {
            cout << "Эмулятор работает в режиме пошагового выполнения команд" << endl;
            cout << "Для выполнения следующей команды нажимайте <ПРОБЕЛ>" << endl;
            cout << "Для отключения/включения пошагового режима нажмите <P> (английская)" << endl;
            cout << "Для вывода на экран содержимого регистров и памяти нажмите <TAB>" << endl;
            cout << "Для для инвертирования сигнала TEST нажмите <T> (английская)" << endl;
        }
        else
        {
            cout << "Step by step mode ON" << endl;
            cout << "Press <SPACE> for next command" << endl;
            cout << "To turn ON/OFF step mode press <P>" << endl;
            cout << "To display the contents of registers and memory, press <TAB>" << endl;
            cout << "To invert TEST  signal press <T>" << endl;
        }
        cout << endl;
    }
    else {
        if (RU_lang) {
            cout << "Режим пошагового выполнения команд отключен" << endl;
            cout << "Для вывода на экран состояния ламп нажмите <TAB>" << endl;
        }
        else
        {
            cout << "Step by step mode OFF" << endl;
            cout << "To display lamp states, press <TAB>" << endl;
        }

    }

    //инициализация регистров
    ACC = 0; //Accumulator
    CF = false; //Carry bit
    TEST = false; //test signal
    RC = 0; //Register Control
    program_counter = 0; //инициализация счетчика команд

    //инициализация ячеек RAM
    ram.resize(16);
    for(int i = 0;i<16; i++)
        {
            ram.at(i).stat_char.resize(4);
            ram.at(i).cell.resize(16);
        }
    
    for (int i = 0; i < 16; i++) { registers.push_back(0);} //инициализация регистров
    ram_ports.resize(2); // инициализация портов
    rom_ports.resize(2);

    //основной цикл программы
    while (1)
    {
        go_forward = false;
        printer.sync(); //синхроимпульс для принтера

        //мониторинг нажатия клавиш в обычном режиме
        if (_kbhit())
        {
            int pressed_key = _getch();
            //проверяем нажатие кнопки P
            if (pressed_key == 112 || pressed_key == 167 || pressed_key == 80 || pressed_key == 135) step_mode = !step_mode;
            keyboard.press_key(pressed_key);

            //выводим содержимое регистров если эмулятор работает в обычном режиме
            if (!step_mode && !log_to_console)
            {
                if (pressed_key == 9) //нажата TAB
                    print_all();
            }

        }

        //задержка вывода по нажатию кнопки в пошаговом режиме
        while (!go_forward && step_mode)
        {
            //засыпаем, чтобы не загружать процессор
            while (!_kbhit()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            int pressed_key = _getch();
            if (pressed_key == 0) pressed_key = _getch();
            //cout << pressed_key << endl;
            if (pressed_key == 32) go_forward = true;
            if (pressed_key == 9)
              {
                print_all();
              }
            if (pressed_key == 116 || pressed_key == 84 || pressed_key == 165 || pressed_key == 133)
            {
                TEST = !TEST; //инвертируем сигнал тест
            }
            if (pressed_key == 112 || pressed_key == 167 || pressed_key == 80 || pressed_key == 135) step_mode = !step_mode;
            if (!step_mode) go_forward = true;
        };
        
        //основной цикл 
        
        //проверка текущего адреса
        if (program_counter >= commands.size()) 
        { 
            SetConsoleTextAttribute(hConsole, 2);
            cout << "Program_counter out of rande" << endl;
            SetConsoleTextAttribute(hConsole, 7); 
            break; 
        }
        
        //выводим текущую команду
        if (log_to_console)
        {
            if (comments.at(program_counter).size() > 1) cout << endl << comments.at(program_counter) << endl << endl;
            cout << program_counter << "\t" << commands.at(program_counter) << "\t";
        }
        
        //декодер комманд
        
        // NOP
        if (commands.at(program_counter) == 0)
        {
            program_counter++;
            if(log_to_console) cout << "No operation" << endl;
            continue;
        }
              
        // 0001xxxx AAAAAAAA условный переход
        if ((commands.at(program_counter) >> 4) == 1)
        {
        
            bool condition = false; //признак выполнения условий

            //проверка условия ACC = 0
            if ((commands.at(program_counter) & 4) == 4 && ACC == 0) condition = true;
            //проверка условия CF = 1
            if ((commands.at(program_counter) & 2) == 2 && CF) condition = true;
            //проверка условия TEST = 0
            if ((commands.at(program_counter) & 1) == 1 && !TEST) condition = true;
            //инверсия флагов
            if ((commands.at(program_counter) & 8) == 8 && (commands.at(program_counter) & 7) != 0) condition = !condition;

            //строка условий
            string con = "(";
            if ((commands.at(program_counter) & 8) == 8) con += "NOT";
            con += "(";
            if ((commands.at(program_counter) & 4) == 4) con += "ACC=0 ";
            if ((commands.at(program_counter) & 2) == 2) con += "CF=1 ";
            if ((commands.at(program_counter) & 1) == 1) con += "TEST=0";
            con += "))";

            //переход к указанному адресу
            if (condition) 
            {
                program_counter = (program_counter >> 8) * 256 + commands.at(program_counter + 1);
                if (log_to_console) cout << "conditional " << con << " jump to " << program_counter << endl;
                continue;
            }
            
            // если условие не выполнено - продолжаем
            program_counter += 2;
            if (log_to_console) cout << "condition " << con << " fail, continue" << endl;
            continue;
        }
        
        // 0100xxxx AAAAAAAA безусловный переход
        if ((commands.at(program_counter) >> 4) == 4)
        {
            

            //замена счетчика
            program_counter = (commands.at(program_counter) & 15) * 256 + commands.at(program_counter + 1);

            if (log_to_console) cout << "jump to " << program_counter << endl;
                       
            continue;
        }

        // 0101xxxx AAAAAAAA переход к подпрограмме
        if ((commands.at(program_counter) >> 4) == 5)
        {
            //сохранение адреса возврата в стеке
            if (stack.size() == 3)
            {
                stack.at(0) = stack.at(1); // затираем самый старый элемент
                stack.at(1) = stack.at(2);
                stack.pop_back();
            }

            stack.push_back(program_counter + 2);

            //замена счетчика
            program_counter = (commands.at(program_counter) & 15) * 256 + commands.at(program_counter + 1);
            if (log_to_console) cout << "jump to subroutine at " << program_counter << endl;
            continue;
        }

        // 1100xxxx возврат из подпрограммы
        if ((commands.at(program_counter) >> 4) == 12)
        {
            //загрузка кода возврата в аккумулятор
            ACC = commands.at(program_counter) & 15;

            if (stack.size() == 0)
            {
                //пустой стек
                if (log_to_console) cout << "stack empty!" << endl;
                //print_all();
                break;
            }

            //загрузка адреса возврата из стека
            program_counter = stack.back();
            stack.pop_back();

            if (log_to_console) cout << "return to " << program_counter << " (ACC=" << ACC  << ") stack size is " << stack.size() << endl;
            continue;
        }

        //0111rrrr Increment and Skip 
        if ((commands.at(program_counter) >> 4) == 7)
        {
            int r = commands.at(program_counter) & 15; //номер регистра
            registers.at(r)++;
            if (registers.at(r) == 16) registers.at(r) = 0;
            if (registers.at(r) != 0)
            {
                //переход по адресу в следующей ячейке
                int new_adress = (program_counter >> 8) * 256 + commands.at(program_counter + 1);
                program_counter = new_adress;
                if (log_to_console) cout << "increment R" << r << "(" << registers.at(r) << ") and jump to " << new_adress << endl;
                continue;
            }

            if (log_to_console) cout << "increment R" << r << "(" << registers.at(r) << ") and continue " << endl;

            //переход к следующей команде
            program_counter += 2;
            continue;
        }

        // 0010xxx0 Fetch Immediate
        if ((commands.at(program_counter) >> 4) == 2 && (commands.at(program_counter) & 1) == 0)
        {
            int r = commands.at(program_counter) & 14; //номер регистра

            registers.at(r) = commands.at(program_counter + 1) >> 4;
            registers.at(r+1) = commands.at(program_counter + 1) & 15;

            //OutputDebugStringW(L"Fetch Immediate \n");
            if (log_to_console) cout << "load " << commands.at(program_counter + 1) << " to R" << r << "R" << r+1 << endl;

            //переход к следующей команде
            program_counter += 2;
            continue;
        }
        
        // 0011xxx0 Fetch Indirect
        if ((commands.at(program_counter) >> 4) == 3 && (commands.at(program_counter) & 1) == 0)
        {
            int r = commands.at(program_counter) & 14; //номер регистра
            int mem_addr = (program_counter >> 8) * 256 + registers.at(0) * 16 + registers.at(1);
            //проверка на выход адреса за размер массива
            if (mem_addr > commands.size() - 1) { cout << "Address out of range!"; break; }
            int mem_data = commands.at(mem_addr);
            registers.at(r) = mem_data >> 4;
            registers.at(r + 1) = mem_data & 15;

            if (log_to_console) cout << "load to R" << r << "R" << r+1 << " = " << mem_data << " at ADDR(" << mem_addr << ")" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        // 0011xxx1 jump Indirect
        if ((commands.at(program_counter) >> 4) == 3 && (commands.at(program_counter) & 1) == 1)
        {
            int r = commands.at(program_counter) & 14; //номер регистра
            int mem_addr = (program_counter >> 8) * 256 + registers.at(r) * 16 + registers.at(r+1);

            if (log_to_console) cout << "jump to addr in R" << r << "R" << r + 1 << " = " << mem_addr << endl;

            //переход к следующей команде
            program_counter = mem_addr;
            continue;
        }

        //1010rrrr Load (register to ACC) 
        if ((commands.at(program_counter) >> 4) == 10)
        {
            int r = commands.at(program_counter) & 15; //номер регистра
            ACC = registers.at(r);

            if (log_to_console) cout << "ACC = R" << r << "(" << registers.at(r) << ")" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1000rrrr ADD (register + ACC -> ACC) 
        if ((commands.at(program_counter) >> 4) == 8)
        {
            int r = commands.at(program_counter) & 15; //номер регистра
            ACC = ACC + registers.at(r) + CF;
            if (ACC > 15)
            {
                CF = true;
                ACC = ACC & 15;
            }
            else CF = false;

            if (log_to_console) cout << "ACC + R" << r << " = " << ACC << " CF=" << CF << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1000rrrr SUB (ACC - R - CF -> ACC, CF) 
        if ((commands.at(program_counter) >> 4) == 9)
        {
            int r = commands.at(program_counter) & 15; //номер регистра
            int old_ACC = ACC;
            int CF_old = CF;
            ACC = ACC - registers.at(r) - CF;
            if (ACC < 0)
            {
                CF = false;
                ACC = ACC + 16;
            }
            else CF = true;

            if (log_to_console) cout << "ACC(" << old_ACC << ") - R" << r << "(" << registers.at(r) << ") - CF(" << CF_old << ") = " << ACC << " CF = " << CF << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }
        
        //1101dddd загрузка dddd в аккумулятор
        if ((commands.at(program_counter) >> 4) == 13)
        {
            ACC = commands.at(program_counter) & 15;
            if (log_to_console) cout << "load " << ACC << " to ACC " << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //0010rrr1 загрузка адреса в RC
        if ((commands.at(program_counter) >> 4) == 2 && (commands.at(program_counter) & 1) == 1)
        {
            int r = commands.at(program_counter) & 14; //номер первого регистра

            RC = registers.at(r) * 16 + registers.at(r + 1);

            if (log_to_console) cout << "load R" << r << "R" << r+1 << "(" << RC << ") to RC" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //0110rrrr Increment (увеличить регистр) 
        if ((commands.at(program_counter) >> 4) == 6)
        {
            int r = commands.at(program_counter) & 15; //номер регистра
            registers.at(r)++;
            if (registers.at(r) == 16) {
                registers.at(r) = 0; 
            }

            if (log_to_console) cout << "increment R" << r << "(" << registers.at(r) << ") CF = " << CF << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //11110000 CLB обнуляем ACC и CF
        if (commands.at(program_counter) == 240)
        {
            ACC = 0;
            CF = false;
            if (log_to_console) cout << "Clear both " << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //11110001 CLC обнуляем CF
        if (commands.at(program_counter) == 241)
        {
            
            CF = false;
            if (log_to_console) cout << "Clear carry " << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1011rrrr XCH ACC <-> RRRR
        if ((commands.at(program_counter) >> 4) == 11)
        {
            int r = commands.at(program_counter) & 15; //номер регистра
            int tmp = registers.at(r);
            registers.at(r) = ACC;
            ACC = tmp;

            if (log_to_console) cout << "Exchange ACC(" << ACC << ") <-> R" << r << "(" << registers.at(r) << ")" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }
        
        //1110 0001 Write RAM Port
        if (commands.at(program_counter)  == 225)
         {
            int port = RC >> 6;
            ram_ports.at(port) = ACC;
            string binary = to_string((ACC >> 3) & 1) + to_string((ACC >> 2) & 1) + to_string((ACC >> 1) & 1) + to_string(ACC & 1);
            if (log_to_console) cout << "ACC(" << ACC << ") -> RAM Port " << port << " [" << binary << "]" << endl;
            
            // если порт = 0, отдаем данные принтеру
            if (port == 0) printer.put_data(ACC);

            //переход к следующей команде
            program_counter += 1;
            continue;
         }

        //1110 0010 Write ROM Port
        if (commands.at(program_counter) == 226)
        {
            int port = RC >> 4;
            rom_ports.at(port) = ACC;
            string binary = to_string((ACC >> 3) & 1) + to_string((ACC >> 2) & 1) + to_string((ACC >> 1) & 1) + to_string(ACC & 1);
            if (log_to_console) cout << "ACC(" << ACC << ") -> ROM Port " << port << " [" << binary << "]" << endl;

            // если порт = 0, отдаем данные принтеру и клавиатуре
            if (port == 0)
            {
                printer.shifter_in(ACC);
                keyboard.shifter_in(ACC);
            }


            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 1010 Read ROM Port
        if (commands.at(program_counter) == 234)
        {
            int port = RC >> 4;
            ACC = 0; //обнуляем ACC

            // если порт = 2, получаем данные от принтера
            if (port == 2) ACC = printer.get_data();
           
            // если порт = 1, получаем данные с клавиатуры
            if (port == 1) ACC = keyboard.get_data();

            string binary = to_string((ACC >> 3) & 1) + to_string((ACC >> 2) & 1) + to_string((ACC >> 1) & 1) + to_string(ACC & 1);
            if (log_to_console) cout << "Read ROM Port " << port << " [" << binary << "]" << endl;
           
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 0000 Write Main Memory
        if (commands.at(program_counter) == 224)
        {
            int selected_reg = RC >> 4;
            ram.at(selected_reg).cell.at(RC & 15) = ACC;

            if (log_to_console) cout << "ACC(" << ACC << ") -> RAM[" << selected_reg << "] Cell[" << (RC & 15) << "]" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 1001 Read Main Memory
        if (commands.at(program_counter) == 233)
        {
            int selected_reg = RC >> 4;
            ACC = ram.at(selected_reg).cell.at(RC & 15);

            if (log_to_console) cout << "ACC(" << ACC << ") = RAM[" << selected_reg << "] Cell[" << (RC & 15) << "]" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 1011 Add Main Memory
        if (commands.at(program_counter) == 235)
        {
            int selected_reg = RC >> 4;
            int ACC_old = ACC;
            int CF_old = CF;
            ACC = ACC + ram.at(selected_reg).cell.at(RC & 15) + CF;
            if (ACC > 15)
            {
                ACC = ACC - 16;
                CF = true;
            }
            else 
            { 
                CF = 0; 
            }

            if (log_to_console) cout << "ADD RAM[" << selected_reg << "] Cell[" << (RC & 15) << "](" << ram.at(selected_reg).cell.at(RC & 15) << ") + ACC("<< ACC_old << ") + CF(" << CF_old << ") = ACC(" << ACC << ") CF = " << CF << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 1000 Subtract Main Memory
        if (commands.at(program_counter) == 232)
        {
            int selected_reg = RC >> 4;
            int ACC_old = ACC;
            int CF_old = CF;
            ACC = ACC - ram.at(selected_reg).cell.at(RC & 15) - CF;
            if (ACC < 0)
            {
                ACC = ACC + 16;
                CF = false;
            } else
            {
                CF = true;
            }

            if (log_to_console) cout << "SUB RAM[" << selected_reg << "] Cell[" << (RC & 15) << "](" << ram.at(selected_reg).cell.at(RC & 15) << ") from ACC(" << ACC_old <<")- CF(" << CF_old << ") = ACC(" << ACC << ") CF = " << CF << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }
        
        //1111 0010 Increment Accumulator
        if (commands.at(program_counter) == 242)
        {
            
            ACC++;
            if (ACC > 15)
            {
                ACC = ACC - 16;
                CF = true;
            } else
            {
                CF = false;
            }

            if (log_to_console) cout << "Increment ACC(" << ACC << ") CF = " << CF << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }
        
        //1110 0100 Write Status Char 0
        if (commands.at(program_counter) == 228)
        {
            int selected_reg = RC >> 4;
            ram.at(selected_reg).stat_char.at(0) = ACC;

            if (log_to_console) cout << "ACC(" << ACC << ") -> RAM[" << selected_reg << "] Status Char[0]" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 0101 Write Status Char 1
        if (commands.at(program_counter) == 229)
        {
            int selected_reg = RC >> 4;
            ram.at(selected_reg).stat_char.at(1) = ACC;

            if (log_to_console) cout << "ACC(" << ACC << ") -> RAM[" << selected_reg << "] Status Char[1]" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 0110 Write Status Char 2
        if (commands.at(program_counter) == 230)
        {
            int selected_reg = RC >> 4;
            ram.at(selected_reg).stat_char.at(2) = ACC;

            if (log_to_console) cout << "ACC(" << ACC << ") -> RAM[" << selected_reg << "] Status Char[2]" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 0111 Write Status Char 1
        if (commands.at(program_counter) == 231)
        {
            int selected_reg = RC >> 4;
            ram.at(selected_reg).stat_char.at(3) = ACC;

            if (log_to_console) cout << "ACC(" << ACC << ") -> RAM[" << selected_reg << "] Status Char[3]" << endl;

            //переход к следующей команде
            program_counter += 1;
            continue;
        }

       //1111 1000 Decrement Accumulator
        if (commands.at(program_counter) == 248)
        {
            ACC--;
            if (ACC < 0)
            {
                ACC = ACC + 16;
                CF = false;
            } else
            {
                CF = true;
            }

            if (log_to_console) cout << "decrement ACC(" << ACC << ") CF = " << CF << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1111 0101 Rotate Left
        if (commands.at(program_counter) == 245)
        {
            bool tmp_CF = CF;
            CF = ACC & 8;
            ACC = ((ACC << 1) | tmp_CF) & 15;

            if (log_to_console) cout << "RAL CF = " << CF << " ACC = " << ACC << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1111 0110 Rotate Right
        if (commands.at(program_counter) == 246)
        {
            int tmp_CF = CF;
            CF = ACC & 1;
            ACC = (ACC >> 1) | (tmp_CF << 3);

            if (log_to_console) cout << "RAR CF = " << CF << " ACC = " << ACC << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }
        
        //1111 0011 Complement Carry
        if (commands.at(program_counter) == 243)
        {
            CF = !CF;

            if (log_to_console) cout << "invert CF = " << CF <<  endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1111 0100 Complement Accumulator
        if (commands.at(program_counter) == 244)
        {
            ACC = 15 - ACC;

            if (log_to_console) cout << "invert ACC = " << ACC <<  endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 1100 Read Status Char 0
        if (commands.at(program_counter) == 236)
        {
            int selected_reg = RC >> 4;
            ACC = ram.at(selected_reg).stat_char.at(0);

            if (log_to_console) cout << "ACC(" << ACC << ") = RAM[" << selected_reg << "] Status Char[0]" << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 1101 Read Status Char 1
        if (commands.at(program_counter) == 237)
        {
            int selected_reg = RC >> 4;
            ACC = ram.at(selected_reg).stat_char.at(1);

            if (log_to_console) cout << "ACC(" << ACC << ") = RAM[" << selected_reg << "] Status Char[1]" << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 1110 Read Status Char 2
        if (commands.at(program_counter) == 238)
        {
            int selected_reg = RC >> 4;
            ACC = ram.at(selected_reg).stat_char.at(2);

            if (log_to_console) cout << "ACC(" << ACC << ") = RAM[" << selected_reg << "] Status Char[2]" << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1110 1111 Read Status Char 3
        if (commands.at(program_counter) == 239)
        {
            int selected_reg = RC >> 4;
            ACC = ram.at(selected_reg).stat_char.at(3);

            if (log_to_console) cout << "ACC(" << ACC << ") = RAM[" << selected_reg << "] Status Char[3]" << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1111 0111 Transfer Carry and Clear
        if (commands.at(program_counter) == 247)
        {
            ACC = CF;
            CF = false;

            if (log_to_console) cout << "TCC ACC = CF (" << ACC << " ), CF = 0" << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1111 1001 Transfer Carry Subtract	TCS
        if (commands.at(program_counter) == 249)
        {
            if (CF)
            {
                ACC = 10;
            }
            else
            {
                ACC = 9;
            }
            
            CF = false;

            if (log_to_console) cout << "TCS ACC = " << ACC << ", CF = 0" << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //1111 1010 Set Carry
        if (commands.at(program_counter) == 250)
        {
            CF = true;

            if (log_to_console) cout << "STC CF = 1" << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }
     
        //1111 1011 Decimal Adjust Accumulator
        if (commands.at(program_counter) == 251)
        {
            if (ACC > 9 || CF)
            {
                ACC = ACC + 6;
                if (ACC > 15)
                {
                    ACC = ACC - 16;
                    CF = true;
                }
            }
            
            if (log_to_console) cout << "DAA ACC = " << ACC << ", CF = " << CF << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }

        //300 print state
        if (commands.at(program_counter) == 300)
        {
            //печатаем состояние и ждем нажатия
            if (log_to_console) cout << "Print and wait" << endl;
            print_all();
            SetConsoleTextAttribute(hConsole, 10);
            cout << "Press a key..." << endl;
            SetConsoleTextAttribute(hConsole, 7);

            //step_mode = true;
            while (!_kbhit()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            program_counter += 1;
            continue;
        }

        //301 exit
        if (commands.at(program_counter) == 301)
        {
            //выход из программы
            if (log_to_console) cout << "Exit program..." << endl;
            return 0;
        }

        //1111 1100 Keyboard Process
        if (commands.at(program_counter) == 252)
        {
            switch (ACC)
            {
            case 0:
                break;
            case 1:
                ACC = 1;
                break;
            case 2:
                ACC = 2;
                break;
            case 4:
                ACC = 3;
                break;
            case 8:
                ACC = 4;
                break;
            default:
                ACC = 15;
            }

            if (log_to_console) cout << "KBP ACC = " << ACC << ")" << endl;
            //переход к следующей команде
            program_counter += 1;
            continue;
        }
 

        SetConsoleTextAttribute(hConsole, 4);
        cout << "Unknown command (" << commands.at(program_counter) << ")! ";
        SetConsoleTextAttribute(hConsole, 7);
        cout << "Program counter = " << program_counter << endl;
        break;
    }

    return 0;
}

void print_all()
{
    //выводим значения всех регистров и памяти

    cout << "================================================================" << endl;
    cout << "ACC = " << ACC << " CarryFlag (CF) = " << CF << " TEST = " << TEST << "    ";
    if ((ram_ports.at(1) & 1) == 1) { SetConsoleTextAttribute(hConsole, 10); } else { SetConsoleTextAttribute(hConsole, 8); }
    cout << " MEMORY ";
    SetConsoleTextAttribute(hConsole, 7);
    if ((ram_ports.at(1) & 2) == 2) { SetConsoleTextAttribute(hConsole, 12); } else { SetConsoleTextAttribute(hConsole, 8); }
    cout << " OVERFLOW ";
    SetConsoleTextAttribute(hConsole, 7);
    if ((ram_ports.at(1) & 4) == 4) {SetConsoleTextAttribute(hConsole, 10); } else { SetConsoleTextAttribute(hConsole, 8); }
    cout << " MINUS ";
    SetConsoleTextAttribute(hConsole, 7);
    cout << endl;
    cout << "================================================================" << endl;
    
    if (log_to_console) // выводим регистры только в режиме пошагового исполнения
    {

        for (int i = 0; i < 16; i += 2)
        {
            cout << "R" << i << "R" << i + 1 << "\t= ";
            if (registers.at(i) < 10) cout << " ";
            cout << registers.at(i) << " ";
            if (registers.at(i + 1) < 10) cout << " ";
            cout << registers.at(i + 1) << " total ";
            if (registers.at(i) * 16 + registers.at(i + 1) < 10) cout << " ";
            if (registers.at(i) * 16 + registers.at(i + 1) < 100) cout << " ";
            cout << registers.at(i) * 16 + registers.at(i + 1) << endl;
        }
        cout << "======================================================================================" << endl;
        cout << "REG|  3   2   1   0 | 15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0 " << endl;
        cout << "======================================================================================" << endl;
        for (int i = 0; i < 8; i += 1)
        {
            cout << "R" << i << " | ";
            if (ram.at(i).stat_char.at(3) < 10) cout << " ";
            cout << ram.at(i).stat_char.at(3) << "  ";
            if (ram.at(i).stat_char.at(2) < 10) cout << " ";
            cout << ram.at(i).stat_char.at(2) << "  ";
            if (ram.at(i).stat_char.at(1) < 10) cout << " ";
            cout << ram.at(i).stat_char.at(1) << "  ";
            if (ram.at(i).stat_char.at(0) < 10) cout << " ";
            cout << ram.at(i).stat_char.at(0) << " | ";
            for (int j = 15; j >= 0; j--)
            {
                if (ram.at(i).cell.at(j) < 10) cout << " ";
                cout << ram.at(i).cell.at(j) << "  ";
            }
            cout << endl;
        }

        cout << "======================================================================================" << endl;
        cout << "Stack: " << stack.size() << endl;
        for (int i = 0; i < stack.size(); i++)
        {
            cout << i << " " << stack.at(i) << endl;
        }
        cout << "======================================================================================" << endl;
        cout << "Printer shifter: " << printer.get_shifter() << endl;
        cout << "Keyboard shifter: " << keyboard.get_shifter() << endl;
        cout << "======================================================================================" << endl;
    }

}

void list()
{
    cout << endl;
    if (RU_lang) 
    {
        cout << "Включена опция листинга программы" << endl;
        cout << "-------------------------------------------------" << endl;
        cout << " N стр" << endl;
        cout << "DEC/HEX" << "\t\t" << "Код" << "\t" << "Операция" << "\t" << endl;
        cout << "-------------------------------------------------" << endl;
    }
    else
    {
        cout << "Listing in ON" << endl;
        cout << "-------------------------------------------------" << endl;
        cout << "Line N" << endl;
        cout << "DEC/HEX" << "\t\t" << "OPcode" << "\t" << "Operation" << "\t" << endl;
        cout << "-------------------------------------------------" << endl;
    }
    
    for (int i = 0; i < commands.size(); ++i)
    {
        if (comments.at(i).length() > 0)
        {
            cout << comments.at(i) << endl;
        }
        
        if (i < 10) cout << " ";
        if (i < 100) cout << " ";
        cout << i << " " << hex;
        if (i < 16) cout << " ";
        if (i < 256) cout << " ";
        cout << i << dec << "\t\t" << commands.at(i) << "\t";

        // NOP
        if (commands.at(i) == 0) cout << "NOP";

        // 0001xxxx AAAAAAAA условный переход
        if ((commands.at(i) >> 4) == 1)
        {
            cout << "JMP IF ";
            //инверсия флагов
            if ((commands.at(i) & 8) == 8) cout << "1";
            else cout << "0";
            //проверка условия ACC = 0
            if ((commands.at(i) & 4) == 4) cout << "1";
            else cout << "0";
            //проверка условия CF = 1
            if ((commands.at(i) & 2) == 2) cout << "1";
            else cout << "0";
            //проверка условия TEST = 0
            if ((commands.at(i) & 1) == 1) cout << "1";
            else cout << "0";
            cout << " to " << commands.at(i + 1);
            cout << endl;
            i++; continue;
        }

        // 0100xxxx AAAAAAAA безусловный переход
        if ((commands.at(i) >> 4) == 4)
        {
            cout << "JMP to " << commands.at(i + 1) << endl;
            i++; continue;
        }

        // 0101xxxx AAAAAAAA переход к подпрограмме
        if ((commands.at(i) >> 4) == 5)
        {
            cout << "SUB " << commands.at(i + 1) << endl;
            i++; continue;
        }

        // 1100xxxx возврат из подпрограммы
        if ((commands.at(i) >> 4) == 12)
        {
            cout << "RET " + to_string(commands.at(i) & 15);
        }

        //0111rrrr Increment and Skip 
        if ((commands.at(i) >> 4) == 7)
        {
            cout << "ISZ R" + to_string(commands.at(i) & 15) << " " << commands.at(i+1) << endl;
            i++; continue;
        }

        
        // 0010xxx0 Fetch Immediate
        if ((commands.at(i) >> 4) == 2 && (commands.at(i) & 1) == 0)
        {
            int r = commands.at(i) & 14; //номер регистра
            cout << "FIM " << commands.at(i + 1) << " to R" << r << "R" << r + 1 << endl;
            i++; continue;
        }

        // 0011xxx0 Fetch Indirect
        if ((commands.at(i) >> 4) == 3 && (commands.at(i) & 1) == 0)
        {
            int r = commands.at(i) & 14; //номер регистра
            cout << "FIN to R" << r << "R" << r + 1;
        }

        // 0011xxx1 jump Indirect
        if ((commands.at(i) >> 4) == 3 && (commands.at(i) & 1) == 1)
        {
            int r = commands.at(i) & 14; //номер регистра
            cout << "JMP IN R" << r << "R" << r + 1;
        }

        //1010rrrr Load (register to ACC)
        if ((commands.at(i) >> 4) == 10)
        {
            int r = commands.at(i) & 15; //номер регистра
            cout << "LD R" << r << " to ACC";
        }

        //1000rrrr ADD (register + ACC -> ACC)
        if ((commands.at(i) >> 4) == 8)
        {
            int r = commands.at(i) & 15; //номер регистра
            cout << "ADD R" << r;
        }

        //1000rrrr SUB (ACC - R - CF -> ACC, CF)
        if ((commands.at(i) >> 4) == 9)
        {
            int r = commands.at(i) & 15; //номер регистра
            cout << "SUB R" << r;
        }

        //1101dddd загрузка dddd в аккумулятор
        if ((commands.at(i) >> 4) == 13)
        {
            cout << "LDM " << (commands.at(i) & 15) << " to ACC ";
        }
        
        //0010rrr1 загрузка адреса в RC
        if ((commands.at(i) >> 4) == 2 && (commands.at(i) & 1) == 1)
        {
            int r = commands.at(i) & 14; //номер первого регистра
            cout << "SRC R" << r << "R" << r + 1;
        }

        //0110rrrr Increment (увеличить регистр)
        if ((commands.at(i) >> 4) == 6)
        {
            int r = commands.at(i) & 15; //номер регистра
            cout << "INC R" << r;
        }

        //11110000 CLB обнуляем ACC и CF
        if (commands.at(i) == 240)
        {
            //cout << "CLB";
            cout << "Clear both";
        }

        //11110001 CLC обнуляем CF
        if (commands.at(i) == 241)
        {
            //cout << "CLC";
            cout << "CF=0";
        }

        //1011rrrr XCH ACC <-> RRRR
        if ((commands.at(i) >> 4) == 11)
        {
            int r = commands.at(i) & 15; //номер регистра
            cout << "XCH R" << r;
        }

        //1110 0001 Write RAM Port
        if (commands.at(i) == 225)
        {
            cout << "WMP";
        }

        //1110 0010 Write ROM Port
        if (commands.at(i) == 226)
        {
            cout << "WRR";
        }

        //1110 1010 Read ROM Port
        if (commands.at(i) == 234)
        {
            cout << "RDR";
        }

        //1110 0000 Write Main Memory
        if (commands.at(i) == 224)
        {
            cout << "WRITE MEM";
        }

        //1110 1001 Read Main Memory
        if (commands.at(i) == 233)
        {
            cout << "READ MEM";
        }

        //1110 1011 Add Main Memory
        if (commands.at(i) == 235)
        {
            cout << "ADD MEM";
        }

        //1110 1000 Subtract Main Memory
        if (commands.at(i) == 232)
        {
            cout << "SUB MEM";
        }

        //1111 0010 Increment Accumulator
        if (commands.at(i) == 242)
        {
            cout << "INC ACC";
        }
        
        //1110 0100 Write Status Char 0
        if (commands.at(i) == 228)
        {
            cout << "WR0" << endl;
        }

        //1110 0101 Write Status Char 1
        if (commands.at(i) == 229)
        {
            cout << "WR1" << endl;
        }

        //1110 0110 Write Status Char 2
        if (commands.at(i) == 230)
        {
            cout << "WR2" << endl;
        }

        //1110 0111 Write Status Char 1
        if (commands.at(i) == 231)
        {
            cout << "WR3" << endl;
        }

        //1111 1000 Decrement Accumulator
        if (commands.at(i) == 248)
        {
            cout << "DEC ACC";
        }

        //1111 0101 Rotate Left
        if (commands.at(i) == 245)
        {
            cout << "RAL";
        }

        //1111 0110 Rotate Right
        if (commands.at(i) == 246)
        {
            cout << "RAR";
        }

        //1111 0011 Complement Carry
        if (commands.at(i) == 243)
        {
            cout << "!CF";
        }

        //1111 0100 Complement Accumulator
        if (commands.at(i) == 244)
        {
            cout << "!ACC";
        }
        
        //1110 1100 Read Status Char 0
        if (commands.at(i) == 236)
        {
            cout << "RD0" << endl;
        }

        //1110 1101 Read Status Char 1
        if (commands.at(i) == 237)
        {
            cout << "RD1" << endl;
        }

        //1110 1110 Read Status Char 2
        if (commands.at(i) == 238)
        {
            cout << "RD2" << endl;
        }

        //1110 1111 Read Status Char 3
        if (commands.at(i) == 239)
        {
            cout << "RD3" << endl;
        }
        
        //1111 0111 Transfer Carry and Clear
        if (commands.at(i) == 247)
        {
            cout << "TCC";
        }

        //1111 1001 Transfer Carry Subtract	TCS
        if (commands.at(i) == 249)
        {
            cout << "TCS";
        }

        //1111 1010 Set Carry
        if (commands.at(i) == 250)
        {
        cout << "STC" << endl;
        }

        //1111 1011 Decimal Adjust Accumulator
        if (commands.at(i) == 251)
        {
            cout << "DAA";
        }

        //300 print state
        if (commands.at(i) == 300)
        {
            cout << "PRN";
        }

        //301 exit
        if (commands.at(i) == 301)
        {
            cout << "QUIT";
        }

        //1111 1100 Keyboard Process
        if (commands.at(i) == 252)
        {
            cout << "KBP";
        }

        cout << endl;
    }
    cout << "-------------------------------------------------" << endl;
    cout << endl;
};

//определение методов принтера

void Epson::put_data(int d)
{
    bool byte_0 = d & 1;
    bool byte_1 = (d >> 1) & 1;
    bool byte_3 = (d >> 3) & 1;

    if (byte_0) // смена цвета чернил
    {
        ink_color = 1;
        //cout << "printer: inc set red" << endl;
    }
    else
    {
        //ink_color = 0;
        //cout << "printer: inc set black" << endl;
    }

    if (byte_1)  // активация принтера
    {
        //cout << "printer: fire hummers" << endl;
        // "пропечатываем" символы на виртуальном принтере
        for (int i = 3; i < 18; i++)
        {
            if ((((shifter >> 14) >> (17-i)) & 1) == 1)
            {
                //"печатаем" отдельный символ
                paper.back().replace(i-3, 1, drum.at(curr_sector).substr(i-3,1));
            }
        }
        // допечатка правых символов
        for (int i = 0; i < 2; i++)
        {
            if ((((shifter >> 14) >> (17 - i)) & 1) == 1)
            {
                //"печатаем" отдельный символ
                paper.back().replace(i + 16 , 1, drum.at(curr_sector).substr(i + 16, 1));
            }
        }
    }

    if (byte_3)  // протяжка ленты
    {
        if (ink_color == 1) 
        { SetConsoleTextAttribute(hConsole, 12); }
        else { SetConsoleTextAttribute(hConsole, 9); }
        cout << paper.back() << endl; // вывод текущего числа на экран
        SetConsoleTextAttribute(hConsole, 7);
        //paper.push_back("                  "); // добавление новой строки к буферу, если нужно хранить экран вывода отдельно
        paper.back() = "                  ";
        ink_color = 0;// сбрасываем цвет чернил к черному
    }
}

void Epson::shifter_in(int d)
{
    int byte_data = (d >> 1) & 1;
    bool byte_clock = (d >> 2) & 1;
    if (byte_clock)
    {
        shifter = shifter >> 1;
        shifter = shifter | (byte_data << 31);
    }
}

int Epson::get_data()
{
    //отправка данных процессору
    //byte 0 - drum 
    //byte 3 - paper
    int byte = 0;
    //если барабан в позиции 0, сигнал на байте.0 = 1
    if (curr_sector == 0) byte = 1;
    return byte;
}

void Epson::sync()
{
    //если TEST = 0, то через N циклов он становится = 1. И наоборот. Эмуляция вращения барабана в принтере, с которым синхронизируется процессор.
    sync_counter++;
    if (sync_counter > 1000 && TEST == 0)
    {
        TEST = 1;
        sync_counter = 0;
        curr_sector++; // перемещаем сектор на барабане
        if (curr_sector == 13) curr_sector = 0;
    }
    if (sync_counter > 1000 && TEST == 1)
    {
        TEST = 0;
        sync_counter = 0;
    }
}

string Epson::get_shifter()
{
    string out;
    for (int i = 0; i < 18; i++)
    {
        if (((shifter >> (31 - i)) & 1) == 0)
        {
            out = out + "0";
        }
        else
        {
            out = out + "1";
        }
    }
    
    return out;
}

//определение методов клавиатуры

int Genius::get_data()
{
    //отдаем данные в зависимости от нажатой клавиши если совпадает shifter_pattern
    if (key_pressed && ((shifter >> 22) & 0b1111111111) == shifter_pattern)
    {
        key_pressed = false;
        //cout << "key found " << get_shifter() << " + " << bitset<4>(data_for_port) << endl;
        return data_for_port;
    }
    return 0;
}

string Genius::get_shifter()
{
    string out;
    for (int i = 0; i < 10; i++)
    {
        if (((shifter >> (31 - i)) & 1) == 0)
        {
            out = out + "0";
        }
        else
        {
            out = out + "1";
        }
    }

    return out;
}

void Genius::shifter_in(int d)
{
    int byte_data = (d >> 1) & 1;
    bool byte_clock = d & 1;
    if (byte_clock)
    {
        shifter = shifter >> 1;
        shifter = shifter | (byte_data << 31);
    }
}

void Genius::press_key(int key)
{
    //cout << key << "   ";
    
    if (!key_pressed)
    {
        if (key == 48) // key 0
        {
            shifter_pattern = 0b1111110111;
            data_for_port = 0b1000;
            key_pressed = true;
        }
        if (key == 49) // key 1
        {
            shifter_pattern = 0b1111110111;
            data_for_port = 0b0100;
            key_pressed = true;
        }
        if (key == 50) // key 2
        {
            shifter_pattern = 0b1111101111;
            data_for_port = 0b0100;
            key_pressed = true;
        }
        if (key == 51) // key 3
        {
            shifter_pattern = 0b1111011111;
            data_for_port = 0b0100;
            key_pressed = true;
        }
        if (key == 52) // key 4
        {
            shifter_pattern = 0b1111110111;
            data_for_port = 0b0010;
            key_pressed = true;
        }
        if (key == 53) // key 5
        {
            shifter_pattern = 0b1111101111;
            data_for_port = 0b0010;
            key_pressed = true;
        }
        if (key == 54) // key 6
        {
            shifter_pattern = 0b1111011111;
            data_for_port = 0b0010;
            key_pressed = true;
        }
        if (key == 55) // key 7
        {
            shifter_pattern = 0b1111110111;
            data_for_port = 0b0001;
            key_pressed = true;
        }
        if (key == 56) // key 8
        {
            shifter_pattern = 0b1111101111;
            data_for_port = 0b0001;
            key_pressed = true;
        }
        if (key == 57) // key 9
        {
            shifter_pattern = 0b1111011111;
            data_for_port = 0b0001;
            key_pressed = true;
        }
        if (key == 13) // key "=" (Enter)
        {
            shifter_pattern = 0b1101111111;
            data_for_port = 0b1000;
            key_pressed = true;
        }
        if (key == 43) // key "+"
        {
            shifter_pattern = 0b1110111111;
            data_for_port = 0b0010;
            key_pressed = true;
        }
        if (key == 45) // key "-"
        {
            shifter_pattern = 0b1110111111;
            data_for_port = 0b0001;
            key_pressed = true;
        }
        if (key == 47) // key "/"
        {
            shifter_pattern = 0b1101111111;
            data_for_port = 0b0010;
            key_pressed = true;
        }
        if (key == 42) // key "*"
        {
            shifter_pattern = 0b1101111111;
            data_for_port = 0b0100;
            key_pressed = true;
        }
        if (key == 115 || key == 83 || key == 235 || key == 155) // key "SQRT" (S)
        {
            shifter_pattern = 0b1011111111;
            data_for_port = 0b0001;
            key_pressed = true;
        }

        if (key == 99 || key == 67 || key == 225 || key == 145) // key "С" (C)
        {
            shifter_pattern = 0b1111111011;
            data_for_port = 0b1000;
            key_pressed = true;
        }
        if (key == 46) // key "."
        {
            shifter_pattern = 0b1111011111;
            data_for_port = 0b1000;
            key_pressed = true;
        }

        if (key == 111 | key == 79 | key == 233 | key == 135) // key "00" (O)
        {
            shifter_pattern = 0b1111101111;
            data_for_port = 0b1000;
            key_pressed = true;
        }

        if (key == 113 | key == 81 | key == 169 | key == 137) // key "Sign" (Q)
        {
            shifter_pattern = 0b1111111011;
            data_for_port = 0b0001;
            key_pressed = true;
        }
        
        if (key == 97 | key == 65 | key == 228 | key == 148) // key "EX" (A)
        {
            shifter_pattern = 0b1111111011;
            data_for_port = 0b0010;
            key_pressed = true;
        }

        if (key == 122 | key == 90 | key == 239 | key == 159) // key "CE" (Z)
        {
            shifter_pattern = 0b1111111011;
            data_for_port = 0b0100;
            key_pressed = true;
        }

        if (key == 100 | key == 68 | key == 162 | key == 130) // key "#" (D)
        {
            shifter_pattern = 0b1101111111;
            data_for_port = 0b0001;
            key_pressed = true;
        }



        if (key == 224)
        {
            key = _getch();//второй код

            if (key == 82) // key "CM" (Insert)
            {
                shifter_pattern = 0b0111111111;
                data_for_port = 0b0001;
                key_pressed = true;
            }

            if (key == 83) // key "RM" (Del)
            {
                shifter_pattern = 0b0111111111;
                data_for_port = 0b0010;
                key_pressed = true;
            }

            if (key == 71) // key "M+" (Home)
            {
                shifter_pattern = 0b0111111111;
                data_for_port = 0b1000;
                key_pressed = true;
            }

            if (key == 79) // key "M+=" (End)
            {
                shifter_pattern = 0b1011111111;
                data_for_port = 0b1000;
                key_pressed = true;
            }

            if (key == 73) // key "M-" (PgUp)
            {
                shifter_pattern = 0b0111111111;
                data_for_port = 0b0100;
                key_pressed = true;
            }

            if (key == 81) // key "M-=" (PgDn)
            {
                shifter_pattern = 0b1011111111;
                data_for_port = 0b0100;
                key_pressed = true;
            }
        }
    }
}