// Simple C program to display "Hello World"
  
// Header file for input output functions
#include <stdio.h>
#include <json_object.h>
  
// main function -
// where the execution of program begins
int main()
{
	printf(json_object_to_json_string(json_object_new_string("Hello World")));

	return 0;
}