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
	// Initialize with no memory allocated
	m_pData = nullptr;
	m_nSize = 0;
	m_nMax = 0;
	
	// Reserve memory and set size
	Reserve(nSize);
	m_nSize = nSize;
	
	// Initialize all elements with the default value
	for (int i = 0; i < nSize; i++) {
		m_pData[i] = dValue;
	}
}

// copy constructor
DArray::DArray(const DArray& arr) {
	// Initialize with no memory allocated
	m_pData = nullptr;
	m_nSize = 0;
	m_nMax = 0;
	
	// Reserve memory and copy data
	Reserve(arr.m_nSize);
	m_nSize = arr.m_nSize;
	
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
	m_nMax = 0;
}

// free the array
void DArray::Free() {
	if (m_pData != nullptr) {
		delete[] m_pData;
		m_pData = nullptr;
	}
	m_nSize = 0;
	m_nMax = 0;
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
	
	// If we need more space, reserve it
	if (nSize > m_nMax) {
		Reserve(nSize);
	}
	
	// Initialize any new elements to 0 if expanding
	if (nSize > m_nSize) {
		for (int i = m_nSize; i < nSize; i++) {
			m_pData[i] = 0;
		}
	}
	
	// Update the size
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
double& DArray::operator[](int nIndex) {
	if (nIndex < 0 || nIndex >= m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		static double errorValue = 0;
		return errorValue;
	}
	return m_pData[nIndex];
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

// add a new element at the end of the array
void DArray::PushBack(double dValue) {
	// If we need more space, reserve it (double the capacity)
	if (m_nSize >= m_nMax) {
		// If no space yet, allocate initial space
		if (m_nMax == 0) {
			Reserve(1);
		} else {
			Reserve(m_nMax * 2);
		}
	}
	
	// Add the new element at the end
	m_pData[m_nSize] = dValue;
	m_nSize++;
}

// delete an element at some index
void DArray::DeleteAt(int nIndex) {
	if (nIndex < 0 || nIndex >= m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		return;
	}
	
	// Move all elements after the deleted index one position to the left
	for (int i = nIndex + 1; i < m_nSize; i++) {
		m_pData[i - 1] = m_pData[i];
	}
	
	// Decrease the size
	m_nSize--;
}

// insert a new element at some index
void DArray::InsertAt(int nIndex, double dValue) {
	if (nIndex < 0 || nIndex > m_nSize) {
		cout << "Error: Index out of bounds." << endl;
		return;
	}
	
	// If we need more space, reserve it (double the capacity)
	if (m_nSize >= m_nMax) {
		// If no space yet, allocate initial space
		if (m_nMax == 0) {
			Reserve(1);
		} else {
			Reserve(m_nMax * 2);
		}
	}
	
	// Move all elements from the insertion index to the end one position to the right
	for (int i = m_nSize - 1; i >= nIndex; i--) {
		m_pData[i + 1] = m_pData[i];
	}
	
	// Insert the new element
	m_pData[nIndex] = dValue;
	m_nSize++;
}

// allocate enough memory
void DArray::Reserve(int nSize) {
	// If we already have enough space, do nothing
	if (nSize <= m_nMax) {
		return;
	}
	
	// Allocate new memory
	double* pNewData = new double[nSize];
	
	// Copy existing elements to the new memory
	for (int i = 0; i < m_nSize; i++) {
		pNewData[i] = m_pData[i];
	}
	
	// Free the old memory and update pointers
	if (m_pData != nullptr) {
		delete[] m_pData;
	}
	m_pData = pNewData;
	m_nMax = nSize;
}

// overload operator '='
DArray& DArray::operator = (const DArray& arr) {
	// Check for self-assignment
	if (this == &arr) {
		return *this;
	}
	
	// Free the current memory
	Free();
	
	// Reserve new memory and copy data
	Reserve(arr.m_nSize);
	m_nSize = arr.m_nSize;
	
	for (int i = 0; i < m_nSize; i++) {
		m_pData[i] = arr.m_pData[i];
	}
	
	return *this;
}