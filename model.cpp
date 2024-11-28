#include <iostream>
#include <simlib.h>
#include <random>

// Parametry modelu
const double TAX_RATE = 0.2;            // daňová sazba
const double INITIAL_DEBT = 1000000000; // počáteční státní dluh
const double INTEREST_RATE = 0.05;      // úroková sazba na půjčky

// Třída pro simulaci státního rozpočtu
class PublicFinanceModel : public Process
{
public:
    double economicActivity; // Ekonomická aktivita (E)
    double stateRevenue;     // Příjmy státu (T)
    double stateExpenditure; // Výdaje státu (G)
    double stateDebt;        // Státní dluh (D)
    double deficit;          // Deficit rozpočtu

    PublicFinanceModel()
    {
        stateRevenue = 0;
        stateExpenditure = 0;
        stateDebt = INITIAL_DEBT;
        deficit = 0;
        economicActivity = 0;
    }

    // Funkce pro výpočet daní
    void calculateRevenue()
    {
        stateRevenue = economicActivity * TAX_RATE;
    }

    // Funkce pro výpočet výdajů
    void calculateExpenditure()
    {
        stateExpenditure = 0.7 * economicActivity + 50000000; // základní výdaje + podpora infrastruktury
    }

    // Funkce pro výpočet deficitu
    void calculateDeficit()
    {
        deficit = stateExpenditure - stateRevenue;
    }

    // Funkce pro simulaci půjček
    void borrowMoney()
    {
        if (deficit > 0)
        {
            stateDebt += deficit;
            std::cout << "Stát si půjčuje: " << deficit << " (nový dluh: " << stateDebt << ")\n";
        }
    }

    // Funkce pro simulaci změny ekonomické aktivity
    void changeEconomicActivity()
    {
        // Měníme ekonomickou aktivitu náhodně v rozmezí 0-6% (může růst nebo klesat)
        std::random_device rd;
        std::default_random_engine generator(rd());
        std::uniform_real_distribution<double> distribution(-0.03, 0.03);
        economicActivity += distribution(generator);
        if (economicActivity < 0)
            economicActivity = 0; // Ekonomická aktivita nemůže být záporná
        std::cout << "Ekonomická aktivita: " << economicActivity * 100 << "%\n";
    }

    // Hlavní simulace procesu (běh modelu)
    void Behavior()
    {
        while (true)
        {
            changeEconomicActivity();
            calculateRevenue();
            calculateExpenditure();
            calculateDeficit();
            borrowMoney();
            Wait(1); // Každý krok je 1 časová jednotka (rok)
        }
    }
};

int main()
{
    Init(1, 100); // Inicializace simulace
    PublicFinanceModel financeModel;
    financeModel.Activate(); // Aktivace procesu
    Run();                   // Spuštění simulace

    return 0;
}
