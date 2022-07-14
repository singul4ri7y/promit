
<h1 align= "center">Project Promit</h1>
<p align= "center">A programming language inspired by JavaScript and Python!</p>
<div align= "center">
	<a href= "#introduction">Introduction</a>
	.
	<a href= "#learn">Learn</a>
	.
	<a href= "#contribution">Contribution</a>
	.
	<a href= "#why">Why</a>
	.
	<a href= "#install">Install</a>
	.
	<a href= "#build">Build</a>
</div>

## Introduction
Promit is <b>object oriented</b>, <b>bytecode interpreted</b>, <b>lightweight</b>, <b>elegant</b> and <b>fast</b> programming language. It has simple yet <b>aesthetic syntax</b> and easy, <b>condensed</b> library  which helps to tackle down any modern programs.

**Key features :** 
- High-Level Language with dynamically typed syntax.
```dart
// Take the name as an input string.
take name = receive(string);

// Now print the name.
showl 'Your name is : $name!';
```
- Fast, stack-based bytecode interpreter with rich single-pass compiler.
- Fully Object Oriented with classes and instances.
```dart
const cities = [ 'New York', 'Constantinople', 'Sin City (Vegas)' ];

const class Promit {
	const visit(city) {
		showl 'Promit has visited $city!';
	}
};

take promit = Promit();

cities.foreach(fn(city) {
	promit.visit(city);
});

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
		
		super.have();
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

take a = receive(num), b = receive(num);

showl 'Summation of those two numbers is : ${a + b}!';
```
- Minimalist library.
```dart
take dictionary = {
	'name'(const)    :  'SD Asif Hossein',
	'age'            :  19,    // Now, yes.
	'passion'(const) :  'Programming'
};

showl 'key-value pairs are : ';

dictionary.keys()    // Returns a list.
	.foreach(fn(key) {
		showl "$key : ${dictionary[key]}";
});
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

take result = call();   // Result is actually the returned closure.

showl typeof result;    // Expected 'closure'.

showl result();         // Expected "Now it's inner!";
```
 - Has ```continue``` in switch.
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
 - Has loops (``while``, ``for``, ```do ... while```), control flow (``if``, ``else`` and ternary operator ```condition ? expr : else_expr```) and many more!

## Learn

Current **Promit Test Suit** (Located under ``/test`` relative to current directory) has comprehensive test programs to test against **Promit**. Which also can serve as a tutorial to Promit Programming Language, as those files are commented in **Teaching** style. Code more than talk, you know it.

## Contribution
<p>First of all it's a community project. <b>So, feel free to contribute 😀.</b> I will try to merge all the pull request I will get related to bug fixes, feature updates and improvements.</p>

**Follow the below rules of contribution:**
 - All the codes contributed in this repository must be of MIT license.
 - If you are a contributor, leave your fullname, email and your contributed GitHub account username in such manner ``(index) fullname <email> username``. In the next line, please denote your contribution or designation in one line starting with a tab.
 - Leave full details of your contribution in the commit changes log and in the pull request as well.
 - Try leaving comments, as that helps (Like I'm the one to talk).

<b><i>Happy contribution.</i></b>

## Install
<p>Promit comes with compiled interpreter binary with each release, which you can use to run your programs from the command line. With some tweaking with IDE's like <b>NetBeans</b>, <b>VS Code</b>, <b>Atom</b> etc. you can integrate the <b>promit</b> interpreter binary to run programs directly from the IDE. Tested in <b>Windows 7/10/11</b> and <b>Debian 10 (Buster)/ 11 (Bullseye)</b>, <b>Arch Linux</b>, <b>Ubuntu 22.04 LTS</b> and other <b>GNU/Linux<</b>.</p>

**Supported Platforms:** Currently Promit only supports **Linux**, **Windows** and in some point <b>*BSD's</b>. As I do not have a Mac, I could not test the code in MacOS 😅. So, for that reason, **Promit does not support MacOS and current release won't be having any MacOS binary** (Mac users don't hate me).</p>

**Download binary the releases at:** https://github.com/singul4ri7y/promit/releases

**How to install:**
 1. Download your specific platform binary. For example, if I am a Windows user, my preference would be ``promit-<version>-windows-64bit.zip`` file for 64 Bit Windows or ``promit-<version>-windows-32bit.zip`` file for 32 Bit Windows (Isn't 32 bit obsolete?).
 2. Extract the archive ``.tar.gz`` for Linux users and ``.zip`` for Windows users.
 3. Under the ``bin/`` folder the the Promit Interpreter binary resides. Move it to any location which is in the **system path** variable.
     - For Windows users, my recommendation is create a folder named ``MyBIN`` on the disk your Windows is installed on and add the folder location to system path variable (How to add folder location to system path variable? Well it's 21'st century. Google it).
     - For Linux users, you probably already know what to do. My preference is to move the binary to ``/usr/local/bin`` where every user can use it. Or create a new folder to store the binary and add it to system path (Make changes to ``$PATH`` adding your folder location. To make it permanent in do it in the  ``/etc/environment`` file).
 4. Now run any Promit program in the terminal or command prompt using ``promit MyProgram.promit``.

<b><i>Done.</i></b>

## Why
<img src= "logos/logo.jpg" type= "image/jpg" align= "right" width= "350" />

<p align= "justify">Well the idea is very simple. I started my programming journey as a Web Developer. As I got deeper inside the computer science stuff and programming languages, I reinvented my interests in Low-Level things such as kernel development, memory managements, operating systems, compiler design, etc. At the same time, I got into high school (At the time of writing, I'm 19 BTW). I met a very fascinating person, who is one of my dearest friends named <b>Meraj Hossain Promit</b>. He was a real inspiration. A lot of thing went through. Long story short, when I was a first year at high school, I promised him that I would make a full-fledged programming language with the honor of his name under the age of 20. You can see the rest.</p>

## Build
### Prerequisites :
 - ``gcc`` (checked with gcc-10, gcc-11 and gcc-12, any version greater than 6 will do)
 - ``make`` to run the ``Makefile``.
 - ``git`` to clone the repo (Optional).
 
### Installation :
**Arch Linux/Manjaro/Arch Based :**
```arch
pacman --sync git base-devel
```
**Debian/Ubuntu/Debian Based :**
```debian
apt install git build-essential
```
**Windows :**

The only way is to switch to Linux 😁.

Nah, just kidding.

Install ``MinGW-w64``, ``Cygwin`` or ``TDM GCC`` which has ``make`` baked within it.

Install git from : <a href= "https://git-scm.com/downloads">https://git-scm.com/downloads</a>.

### The easy stuff :
Go to any folder, clone the repo and hit ``make``.
```
git clone https://github.com/singul4ri7y/promit
cd promit/promit
make build_release_x64
cd ../bin/Release-x64/
./promit
```
It will start Promit's REPL (Run-Evaluate-Print-Loop) mode. Enjoy the binary.

**Configurations :**
- ``make`` or ``make build_debug_x64`` builds the project in Debug configuration and x64 architecture.
- ``make build_debug_x86`` builds the project in Debug configuration and x86 architecture.
- ``make build_release_x64`` builds the project in Release configuration and x64 architecture.
- ``make build_release_x86`` builds the project in Release configuration and x86 architecture.

<i>Peace.</i>
