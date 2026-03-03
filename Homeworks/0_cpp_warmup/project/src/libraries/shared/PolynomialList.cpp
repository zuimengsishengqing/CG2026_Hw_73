#include "PolynomialList.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>

using namespace std;

PolynomialList::PolynomialList(const PolynomialList& other) {
    // Copy all terms from the other polynomial
    m_Polynomial = other.m_Polynomial;
}

PolynomialList::PolynomialList(const string& file) {
    // Initialize polynomial from file
    ReadFromFile(file);
}

PolynomialList::PolynomialList(const double* cof, const int* deg, int n) {
    // Initialize polynomial from arrays
    for (int i = 0; i < n; i++) {
        if (cof[i] != 0) { // Only add non-zero terms
            AddOneTerm(Term(deg[i], cof[i]));
        }
    }
    compress(); // Sort and merge terms
}

PolynomialList::PolynomialList(const vector<int>& deg, const vector<double>& cof) {
    // Initialize polynomial from vectors
    size_t n = min(deg.size(), cof.size());
    for (size_t i = 0; i < n; i++) {
        if (cof[i] != 0) { // Only add non-zero terms
            AddOneTerm(Term(deg[i], cof[i]));
        }
    }
    compress(); // Sort and merge terms
}

double PolynomialList::coff(int i) const {
    // Find the coefficient of the term with degree i
    for (const auto& term : m_Polynomial) {
        if (term.deg == i) {
            return term.cof;
        }
    }
    return 0.0; // Return 0 if term with degree i doesn't exist
}

double& PolynomialList::coff(int i) {
    // Find the coefficient of the term with degree i
    for (auto& term : m_Polynomial) {
        if (term.deg == i) {
            return term.cof;
        }
    }
    
    // If term with degree i doesn't exist, add it and return its coefficient
    AddOneTerm(Term(i, 0.0));
    compress(); // Sort the list
    
    // Find the newly added term and return its coefficient
    for (auto& term : m_Polynomial) {
        if (term.deg == i) {
            return term.cof;
        }
    }
    
    // This should never happen
    static double error = 0.0;
    return error;
}

void PolynomialList::compress() {
    // Sort terms by degree in descending order
    m_Polynomial.sort([](const Term& a, const Term& b) {
        return a.deg > b.deg;
    });
    
    // Merge terms with the same degree
    auto it = m_Polynomial.begin();
    while (it != m_Polynomial.end()) {
        auto nextIt = it;
        nextIt++;
        
        // Remove zero coefficient terms
        if (it->cof == 0) {
            it = m_Polynomial.erase(it);
            continue;
        }
        
        // Merge terms with the same degree
        while (nextIt != m_Polynomial.end() && it->deg == nextIt->deg) {
            it->cof += nextIt->cof;
            nextIt = m_Polynomial.erase(nextIt);
        }
        
        it++;
    }
}

PolynomialList PolynomialList::operator+(const PolynomialList& right) const {
    PolynomialList result(*this); // Copy left operand
    
    // Add all terms from right operand
    for (const auto& term : right.m_Polynomial) {
        result.AddOneTerm(term);
    }
    
    result.compress(); // Sort and merge terms
    return result;
}

PolynomialList PolynomialList::operator-(const PolynomialList& right) const {
    PolynomialList result(*this); // Copy left operand
    
    // Subtract all terms from right operand
    for (const auto& term : right.m_Polynomial) {
        result.AddOneTerm(Term(term.deg, -term.cof));
    }
    
    result.compress(); // Sort and merge terms
    return result;
}

PolynomialList PolynomialList::operator*(const PolynomialList& right) const {
    PolynomialList result;
    
    // Multiply each term from left operand with each term from right operand
    for (const auto& leftTerm : m_Polynomial) {
        for (const auto& rightTerm : right.m_Polynomial) {
            // Add the product term to the result
            result.AddOneTerm(Term(leftTerm.deg + rightTerm.deg, leftTerm.cof * rightTerm.cof));
        }
    }
    
    result.compress(); // Sort and merge terms
    return result;
}

PolynomialList& PolynomialList::operator=(const PolynomialList& right) {
    if (this != &right) { // Check for self-assignment
        m_Polynomial = right.m_Polynomial; // Copy all terms
    }
    return *this;
}

void PolynomialList::Print() const {
    if (m_Polynomial.empty()) {
        cout << "0" << endl;
        return;
    }
    
    bool firstTerm = true;
    for (const auto& term : m_Polynomial) {
        if (term.cof == 0) continue; // Skip zero coefficient terms
        
        if (!firstTerm && term.cof > 0) {
            cout << "+";
        }
        
        if (term.deg == 0) {
            cout << term.cof;
        } else if (term.deg == 1) {
            if (term.cof == 1) {
                cout << "x";
            } else if (term.cof == -1) {
                cout << "-x";
            } else {
                cout << term.cof << "x";
            }
        } else {
            if (term.cof == 1) {
                cout << "x^" << term.deg;
            } else if (term.cof == -1) {
                cout << "-x^" << term.deg;
            } else {
                cout << term.cof << "x^" << term.deg;
            }
        }
        
        firstTerm = false;
    }
    cout << endl;
}

bool PolynomialList::ReadFromFile(const string& file) {
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
                    AddOneTerm(Term(degree, coefficient));
                }
            } else {
                cerr << "Error: Invalid term format" << endl;
                return false;
            }
        } else {
            cerr << "Error: Not enough terms in file" << endl;
            return false;
        }
    }
    
    inputFile.close();
    compress(); // Sort and merge terms
    return true;
}

PolynomialList::Term& PolynomialList::AddOneTerm(const Term& term) {
    // Add a new term to the polynomial
    m_Polynomial.push_front(term); // Add to front for efficiency
    
    // Return reference to the added term
    // Note: This returns a reference to the first element, which may not be the added term after compression
    // This is a limitation of the current design
    return m_Polynomial.front();
}