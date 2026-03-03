#include "PolynomialMap.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <cmath>

// 指定源文件字符编码为UTF-8
#pragma execution_character_set("utf-8")

using namespace std;

// 拷贝构造函数：创建一个与另一个多项式相同的新多项式
// 使用map的拷贝构造函数，自动复制所有键值对
PolynomialMap::PolynomialMap(const PolynomialMap& other) {
    // Direct copy of map container, map's copy constructor handles all details
    m_Polynomial = other.m_Polynomial;
}

// Initialize polynomial from file
// File format: first line "P number_of_terms", following lines "degree coefficient"
PolynomialMap::PolynomialMap(const string& file) {
    ReadFromFile(file); // Call file reading function
}

// Initialize polynomial from arrays
// Parameters: coefficient array, degree array, number of terms
PolynomialMap::PolynomialMap(const double* cof, const int* deg, int n) {
    // Iterate through all terms, add non-zero coefficient terms to map
    for (int i = 0; i < n; i++) {
        if (cof[i] != 0) { // Only add non-zero terms to avoid unnecessary storage
            m_Polynomial[deg[i]] = cof[i]; // Map automatically handles same degree cases
        }
    }
}

// Initialize polynomial from vectors
// Parameters: degree vector, coefficient vector
PolynomialMap::PolynomialMap(const vector<int>& deg, const vector<double>& cof) {
    assert(deg.size() == cof.size()); // Ensure both vectors have the same length
    
    // Iterate through all terms, add non-zero coefficient terms to map
    for (size_t i = 0; i < deg.size(); i++) {
        if (cof[i] != 0) { // Only add non-zero terms
            m_Polynomial[deg[i]] = cof[i]; // Map automatically handles same degree cases
        }
    }
}

// Get coefficient of specified degree (const version)
// Return 0 if degree doesn't exist
// map's find method returns iterator, returns end() if not found
double PolynomialMap::coff(int i) const {
    auto it = m_Polynomial.find(i); // Find degree i in map
    if (it != m_Polynomial.end()) {
        return it->second; // Found, return corresponding coefficient
    }
    return 0.0; // Not found, return 0
}

// Get coefficient of specified degree (non-const version, allows modification)
// If degree doesn't exist, creates a new term and returns reference to its coefficient
double& PolynomialMap::coff(int i) {
    // map's operator[] automatically creates a new element when key doesn't exist
    // This allows us to directly modify coefficient even if degree didn't exist before
    return m_Polynomial[i];
}

// Compress polynomial: remove all terms with zero coefficient
// For map implementation, this function mainly maintains interface consistency with list implementation
// Map won't have duplicate degrees, so no need to merge like terms
void PolynomialMap::compress() {
    // Iterate through map, remove terms with zero coefficient
    // Note: special handling needed when deleting elements during iteration
    auto it = m_Polynomial.begin();
    while (it != m_Polynomial.end()) {
        if (it->second == 0.0) {
            // Delete term with zero coefficient and get next iterator
            it = m_Polynomial.erase(it);
        } else {
            // Move to next element
            ++it;
        }
    }
}

// Polynomial addition operator overload
// Implementation principle: add coefficients of corresponding terms from two polynomials
PolynomialMap PolynomialMap::operator+(const PolynomialMap& right) const {
    PolynomialMap result(*this); // Create result polynomial, initialized as left operand
    
    // Iterate through each term of right operand
    for (const auto& term : right.m_Polynomial) {
        // Add coefficient of right operand to corresponding term of result polynomial
        result.m_Polynomial[term.first] += term.second;
    }
    
    result.compress(); // Remove possible zero coefficient terms
    return result;
}

// Polynomial subtraction operator overload
// Implementation principle: negate each term of right operand, then add to left operand
PolynomialMap PolynomialMap::operator-(const PolynomialMap& right) const {
    PolynomialMap result(*this); // Create result polynomial, initialized as left operand
    
    // Iterate through each term of right operand
    for (const auto& term : right.m_Polynomial) {
        // Subtract coefficient of right operand from corresponding term of result polynomial
        result.m_Polynomial[term.first] -= term.second;
    }
    
    result.compress(); // Remove possible zero coefficient terms
    return result;
}

// Polynomial multiplication operator overload
// Implementation principle: use distributive law, multiply each term of left operand with each term of right operand
// Product term's degree is sum of two terms' degrees, coefficient is product of two terms' coefficients
PolynomialMap PolynomialMap::operator*(const PolynomialMap& right) const {
    PolynomialMap result; // Create empty result polynomial
    
    // Double loop: iterate through each term of left operand
    for (const auto& leftTerm : m_Polynomial) {
        // Iterate through each term of right operand
        for (const auto& rightTerm : right.m_Polynomial) {
            // Calculate degree and coefficient of product term
            int newDeg = leftTerm.first + rightTerm.first;
            double newCof = leftTerm.second * rightTerm.second;
            
            // Add product term to result polynomial
            result.m_Polynomial[newDeg] += newCof;
        }
    }
    
    result.compress(); // Remove possible zero coefficient terms
    return result;
}

// Assignment operator overload
// Implement deep copy to ensure two polynomial objects are completely independent
PolynomialMap& PolynomialMap::operator=(const PolynomialMap& right) {
    if (this != &right) { // Check for self-assignment
        m_Polynomial = right.m_Polynomial; // Directly copy map container
    }
    return *this;
}

// Print polynomial
// Format: sort by degree from high to low, omit terms with coefficient 1 or -1, correctly handle x^0 and x^1
void PolynomialMap::Print() const {
    if (m_Polynomial.empty()) {
        cout << "0" << endl;
        return;
    }
    
    bool firstTerm = true; // Mark if it's the first term, used to decide whether to show plus sign
    
    // Map defaults to ascending order by key, we need to traverse from high to low
    // Use reverse iterator to traverse from high degree to low degree
    for (auto it = m_Polynomial.rbegin(); it != m_Polynomial.rend(); ++it) {
        int deg = it->first;
        double cof = it->second;
        
        if (cof == 0) continue; // Skip zero coefficient terms
        
        // Handle sign
        if (!firstTerm && cof > 0) {
            cout << "+";
        }
        
        // Handle coefficient
        if (deg == 0) {
            // Constant term
            cout << cof;
        } else if (deg == 1) {
            // Linear term
            if (cof == 1) {
                cout << "x";
            } else if (cof == -1) {
                cout << "-x";
            } else {
                cout << cof << "x";
            }
        } else {
            // Higher degree term
            if (cof == 1) {
                cout << "x^" << deg;
            } else if (cof == -1) {
                cout << "-x^" << deg;
            } else {
                cout << cof << "x^" << deg;
            }
        }
        
        firstTerm = false;
    }
    cout << endl;
}

// Read polynomial from file
// File format: first line "P number_of_terms", following lines "degree coefficient"
bool PolynomialMap::ReadFromFile(const string& file) {
    m_Polynomial.clear(); // Clear current polynomial
    
    ifstream inputFile(file);
    if (!inputFile.is_open()) {
        cerr << "Error: Cannot open file " << file << endl;
        return false;
    }
    
    string line;
    int numTerms = 0;
    
    // Read first line to get number of terms
    if (getline(inputFile, line)) {
        istringstream iss(line);
        char p;
        iss >> p >> numTerms;
        
        if (p != 'P' || numTerms < 0) {
            cerr << "Error: Invalid file format" << endl;
            return false;
        }
    } else {
        cerr << "Error: Empty file" << endl;
        return false;
    }
    
    // Read each term
    for (int i = 0; i < numTerms; i++) {
        if (getline(inputFile, line)) {
            istringstream iss(line);
            int degree;
            double coefficient;
            
            if (iss >> degree >> coefficient) {
                if (coefficient != 0) { // Only add non-zero terms
                    m_Polynomial[degree] = coefficient; // Map automatically handles same degree cases
                }
            } else {
                cerr << "Error: Invalid term format" << endl;
                return false;
            }
        } else {
            cerr << "Error: Insufficient terms in file" << endl;
            return false;
        }
    }
    
    inputFile.close();
    return true;
}