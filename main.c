// Simple C program to display "Hello World"

// Header file for input output functions
#include <stdio.h>
#include "json.h"

// main function -
// where the execution of program begins
int main()
{
	printf("Hello World\r\n");
	fflush(stdout);
	json_object * test_string = json_object_new_string("Hello World");
	const char * json_blob = json_object_to_json_string(test_string);
	printf(json_blob);
	return 0;
}