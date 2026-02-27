// implementation of class DArray
#include "DArray.h"
#include <iostream>
using namespace std;

// default constructor
DArray::DArray() {
	Init();
}

// set an array with default values
DArray::DArray(int nSize, double dValue) {
	// Allocate memory for the array
	m_pData = new double[nSize];
	m_nSize = nSize;
	
	// Initialize all elements with the default value
	for (int i = 0; i < nSize; i++) {
		m_pData[i] = dValue;
	}
}

// copy constructor
DArray::DArray(const DArray& arr) {
	// Allocate memory for the new array
	m_nSize = arr.m_nSize;
	m_pData = new double[m_nSize];
	
	// Copy all elements from the source array
	for (int i = 0; i < m_nSize; i++) {
		m_pData[i] = arr.m_pData[i];
	}
}

// deconstructor
DArray::~DArray() {
	Free();
}

// display the elements of the array
void DArray::Print() const {
	cout << "Array elements: ";
	for (int i = 0; i < m_nSize; i++) {
		cout << m_pData[i];
		if (i < m_nSize - 1) {
			cout << ", ";
		}
	}
	cout << endl;
}
 
// initilize the array
void DArray::Init() {
	m_pData = nullptr;
	m_nSize = 0;
}

// free the array
void DArray::Free() {
	if (m_pData != nullptr) {
		delete[] m_pData;
		m_pData = nullptr;
	}
	m_nSize = 0;
}

// get the size of the array
int DArray::GetSize() const {
	return m_nSize;
}

// set the size of the array
void DArray::SetSize(int nSize) {
	if (nSize < 0) {
		cout << "Error: Array size cannot be negative." << endl;
		return;
	}
	
	// If the new size is the same as the current size, do nothing
	if (nSize == m_nSize) {
		return;
	}
	
	// Allocate new memory
	double* pNewData = new double[nSize];
	
	// Copy existing elements to the new array
	int nCopySize = (nSize < m_nSize) ? nSize : m_nSize;
	for (int i = 0; i < nCopySize; i++) {
		pNewData[i] = m_pData[i];
	}
	
	// Initialize any new elements to 0
	for (int i = nCopySize; i < nSize; i++) {
		pNewData[i] = 0;
	}
	
	// Free the old memory and update pointers
	Free();
	m_pData = pNewData;
	m_nSize = nSize;
}

// get an element at an index
const double& DArray::GetAt(int nIndex) const {
	if (nIndex < 0 || nIndex >= m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		static double errorValue = 0;
		return errorValue;
	}
	return m_pData[nIndex];
}

// set the value of an element 
void DArray::SetAt(int nIndex, double dValue) {
	if (nIndex < 0 || nIndex >= m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		return;
	}
	m_pData[nIndex] = dValue;
}

// overload operator '[]'
const double& DArray::operator[](int nIndex) const {
	if (nIndex < 0 || nIndex >= m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		static double errorValue = 0;
		return errorValue;
	}
	return m_pData[nIndex];
}

// non-const version of operator[]
double& DArray::operator[](int nIndex) {
	if (nIndex < 0 || nIndex >= m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		static double errorValue = 0;
		return errorValue;
	}
	return m_pData[nIndex];
}

// add a new element at the end of the array
void DArray::PushBack(double dValue) {
	// Create a new array with one more element
	double* pNewData = new double[m_nSize + 1];
	
	// Copy existing elements
	for (int i = 0; i < m_nSize; i++) {
		pNewData[i] = m_pData[i];
	}
	
	// Add the new element at the end
	pNewData[m_nSize] = dValue;
	
	// Free the old memory and update pointers
	if (m_pData != nullptr) {
		delete[] m_pData;
	}
	m_pData = pNewData;
	m_nSize++;
}

// delete an element at some index
void DArray::DeleteAt(int nIndex) {
	if (nIndex < 0 || nIndex >= m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		return;
	}
	
	// Create a new array with one less element
	double* pNewData = new double[m_nSize - 1];
	
	// Copy elements before the deleted index
	for (int i = 0; i < nIndex; i++) {
		pNewData[i] = m_pData[i];
	}
	
	// Copy elements after the deleted index
	for (int i = nIndex + 1; i < m_nSize; i++) {
		pNewData[i - 1] = m_pData[i];
	}
	
	// Free the old memory and update pointers
	delete[] m_pData;
	m_pData = pNewData;
	m_nSize--;
}

// insert a new element at some index
void DArray::InsertAt(int nIndex, double dValue) {
	if (nIndex < 0 || nIndex > m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		return;
	}
	
	// Create a new array with one more element
	double* pNewData = new double[m_nSize + 1];
	
	// Copy elements before the insertion index
	for (int i = 0; i < nIndex; i++) {
		pNewData[i] = m_pData[i];
	}
	
	// Insert the new element
	pNewData[nIndex] = dValue;
	
	// Copy elements after the insertion index
	for (int i = nIndex; i < m_nSize; i++) {
		pNewData[i + 1] = m_pData[i];
	}
	
	// Free the old memory and update pointers
	if (m_pData != nullptr) {
		delete[] m_pData;
	}
	m_pData = pNewData;
	m_nSize++;
}

// overload operator '='
DArray& DArray::operator = (const DArray& arr) {
	// Check for self-assignment
	if (this == &arr) {
		return *this;
	}
	
	// Free the current memory
	Free();
	
	// Allocate new memory and copy data
	m_nSize = arr.m_nSize;
	m_pData = new double[m_nSize];
	
	for (int i = 0; i < m_nSize; i++) {
		m_pData[i] = arr.m_pData[i];
	}
	
	return *this;
}