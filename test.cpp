/*  Example Program For Factorial Value Using Recursion In C++
    little drops @ thiyagaraaj.com

    Coded By:THIYAGARAAJ MP             */

#include<iostream>

using namespace std;

//Function
long factorial(int);

int main()
{

     // Variable Declaration
     int n;

     // Get Input Value
     n = 10;

     // Factorial Function Call
     cout<<n<<" Factorial Value Is "<<factorial(n);

     // Wait For Output Screen
     return 0;
 }

// Factorial recursion Function
long factorial(int n)
{
  if (n == 0)
    return 1;
  else
    return(n * factorial(n-1));
}