#ifndef DATABASE_H
#define DATABASE_H

#include <string>

struct Item {
    char* name;
    int amount;
};

void initDatabase();

void saveDatabase();

void loadDatabase();

void addItemToDatabase(const char* name);

void removeItemFromDatabase(const char* name);

void clearDatabase();

void printDatabase();

std::string getDatabaseAsString();

std::string getDatabaseAsStringWithoutAmount();

#endif // DATABASE_H
