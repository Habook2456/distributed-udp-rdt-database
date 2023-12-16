#ifndef BD_H
#define BD_H

#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>

class UDPdatabase
{
private:
    // abstraccion de grafo con unordered_multimap
    std::unordered_multimap<std::string, std::string> data;

public:
    // CRUD
    void createData(const std::string &key, const std::string &value);
    std::vector<std::string> readData(const std::string &key);
    void updateData(const std::string &key, const std::string &oldValue, const std::string &newValue);
    void deleteData(const std::string &key, const std::string &value);
    void showData() const;
    void countData() const;
};

// CREATE - insert (key, value)
void UDPdatabase::createData(const std::string &key, const std::string &value)
{
    data.insert({key, value});
}

// READ - return vector of values (key) 
std::vector<std::string> UDPdatabase::readData(const std::string &key)
{
    // rango de busqueda de datos con la misma clave
    auto range = data.equal_range(key);
    std::vector<std::string> values;
    // insertar valores relacionados con la clave en el vector
    for (auto it = range.first; it != range.second; ++it)
    {
        values.push_back(it->second);
    }
    return values;
}

// UPDATE - update (key, oldValue, newValue)
void UDPdatabase::updateData(const std::string &key, const std::string &oldValue, const std::string &newValue)
{

    std::cout << key << " - " << oldValue << " -> ";
    auto range = data.equal_range(key);
    // buscar valor antiguo
    for (auto it = range.first; it != range.second; ++it)
    {
        if (it->second == oldValue)
        {   
            // actualizar a valor nuevo
            it->second = newValue;
            std::cout << it->first << " - " << it->second << std::endl;
            break;
        }
    }
}

// DELETE - delete (key, value)
void UDPdatabase::deleteData(const std::string &key, const std::string &value)
{
    auto range = data.equal_range(key);
    // buscar valor a eliminar
    for (auto it = range.first; it != range.second; ++it)
    {   // eliminar valor
        if (it->second == value)
        {
            data.erase(it);
            break;
        }
    }
}

// SHOW - print all data
void UDPdatabase::showData() const
{
    for (const auto &pair : data)
    {
        std::cout << "Key: " << pair.first << ", Value: " << pair.second << std::endl;
    }
}

// COUNT - print number of elements
void UDPdatabase::countData() const
{
    std::cout << "Number of elements: " << data.size() << std::endl;
}

#endif // BD_H
