<h1 align= "center">Project Promit</h1>
<p align= "center">A programming language inspired by JavaScript and Python!</p>
<div align= "center">
	<a href= "#introduction">Introduction</a>
	.
	<a href= "#why">Why</a>
	.
	<a href= "#build">Build</a>
</div>

## Introduction
Promit is fully <b>object oriented</b>, <b>bytecode interpreted</b>, <b>lightweight</b>, <b>elegant</b> and <b>fast</b> programming language. It has simple yet <b>aesthetic syntax</b> and easy, <b>condensed</b> library  which helps to tackle down any modern programs.

**Key features :** 
- High-Level Language with dynamically typed syntax.
```dart
// Take the name as an input string.
take name = recieve(string);

// Now print the name.
showl 'Your name is : $name!';
```
- Fast, stack-based bytecode interpreter with rich single-pass compiler.
- Fully Object Oriented with class and instances.
```dart
const cities = [ 'New York', 'Constantinople', 'Sin City (Vegas)' ];

const class Promit {
	const visit(city) {
		showl 'Promit has visited $city!';
	}
};

take promit = Promit();

cities.foreach( fn(city) {
	promit.visit(city);
} );

showl "The variable 'promit' is ${typeof promit} and ${promit instof Promit}.";
```
- Supports inheritance.
```dart
class Breakfast {
	have() {
		showl "Dalicious!";
	}
};

class Soup is Breakfast {
	have() {
		showl "Isn't having soup for breakfast too odd!";
		
		this.have();
	}
};

take breakfast = Soup();

breakfast.have();
```
- Automated memory management with a swift Mark-Sweep garbage collector.
```dart
take string = "This is a string! Rest easy!";

// GC.
```
- Modern string interpolation.
```dart
show "Enter two numbers : ";

take a = recieve(num), b = recieve(num);

showl 'Summation of those two numbers is : ${a + b}!';
```
- Minimalist library.
```dart
take dictionary = {
	'name'(const) :  'SD Asif Hossein',
	'age'(const)  :  19,    // Now, yes.
	'passion'     :  'Programming'
};

dictionary.keys()    // Returns a list.
	.foreach( fn(key) {
		showl "key-value pair is $key : ${dictionary[key]}"
} );
```
- Has functions and closures (Yeah, I know you've already guessed).
```dart
const fn call() {
	take outer = "This is outer!";

	// Closure capturing 'outer' variable.
	return fn() {
		showl outer;
		showl 'Changing outer variable!';
		
		outer = "Now it's inner!";
	
		return outer;
	}
}

take result = call();    // result is actually the returned closure.

showl typeof result;    // Expected 'closure'.

result();
```
 - Has ```continue ``` in switch.
```dart
take bird = 'Duck';

switch(bird) {
	case 'Koel' : {
		showl 'The bird is Koel!';
		break;
	}
	case 'Duck' : {
		showl 'The bird is Duck (maybe)!';
		continue;    // Code will fallthrough to next case.
		showl 'This portion will not be executed!!';
		break;
	}
	case 'Fallthrough' : showl 'Fallthrough!'; break;
	default : break;
}
```
 - Has loops (while, for, do ... while), control flow (if, else and ternary operator ( condition ? expr : else_expr ) and many more!
