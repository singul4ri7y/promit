<h1 align= "center">Project Promit 🚀</h1>
<p align= "center">A neat programming language inspired by JavaScript and Python 👨🏻‍💻</p>
<div align= "center">
    <a href= "#introduction-">Introduction</a>
    .
    <a href= "#learn-">Learn</a>
    .
    <a href= "#contribution-">Contribution</a>
    .
    <a href= "#install-%EF%B8%8F">Install</a>
    .
    <a href= "#why-to-the-author-">Why</a>
    .
    <a href= "#build-%EF%B8%8F">Build</a>
</div>

## Introduction 👾
Promit is **object-oriented**, **bytecode interpreted**, **lightweight**, **elegant** and **fast** programming language. It has simple yet **expressive** syntax with **easy**, **minimalist** and **condensed** library enabling you tackle down any modern programs 🔥.

**Key features 📜:** 
- High-Level Language with dynamically typed syntax 🦾.
```dart
// Take the name as an input string.
take name = receive(string);

// Now print the name.
System::stdout.write_line('Your name is : $name!');

// This is more simpler and less modular way to do it.
showl 'Your name is : $name!';
```
- Fully Object Oriented with classes and instances 🧊.
```dart
const cities = [ 'Venice', 'Constantinople', 'Tokyo', 'Dhaka' ];

const class Promit {
    const visit(city) {
        System::stdout.write_line('Promit has visited $city!');
    }
};

take promit = Promit();

cities.foreach(fn(city) {
    promit.visit(city);
});

System::stdout.write_line("The variable 'promit' is ${typeof promit} and ${promit instof Promit}.");
```
- Supports inheritance 🧬.
```dart
class Breakfast {
    have() {
        System::stdout.write_line("Astronomical!");
    }
};

class Soup is Breakfast {
    have() {
        System::stdout.write_line("Isn't having soup for breakfast too odd!");
        
        super.have();
    }
};

take breakfast = Soup();
breakfast.have();
```
- Automated memory management with a swift Mark-Sweep Garbage Collector ♻️.
```dart
take string = "GC will do the dirty work of MM. Rest easy!";
// GC.
```
- Modern string interpolation 🧶.
```dart
show "Enter two numbers : ";
take a = receive(num), b = receive(num);

System::stdout.write_line('Summation of those two numbers is : ${a + b}!');
```
- Minimalist System Library 🏛️.
```dart
take dictionary = {
    'name'(const)    : 'SD Asif Hossein',
    'age'            : 99.99,    // :3.
    'passion'(const) : 'Programming'
};

showl 'key-value pairs are : ';
dictionary.keys()    // Returns a list.
    .foreach(fn(key) {
        System::stdout.write_line("$key : ${dictionary[key]}");
});
```
- Has functions and closures 🚪.
```dart
const fn call() {
    take outer = "This is outer!";

    // Closure capturing 'outer' variable.
    return fn() {
        System::stdout.write_line(outer);
        System::stdout.write_line('Changing outer variable!');
        
        outer = "Now it's inner!";
    
        return outer;
    }
}

take result = call();   // The returned closure.

System::stdout.write_line(typeof result);    // Expected 'closure'.
System::stdout.write_line(result());         // Expected "Now it's inner!";
```
 - Has **``continue``** in switch 🚀.
```dart
take bird = 'Mockingbird';

switch(bird) {
    case 'Koel' : {
        System::stdout.write_line('The bird is Koel!');
        break;
    }
    
    case 'Mockingbird' : {
        System::stdout.write_line('The bird is Mockingbird!');
        continue;    // Code will fallthrough to next case.
        System::stdout.write_line('This portion will not be executed!!');
        break;
    }
    
    case 'Fallthrough Bird!?' : System::stdout.write_line('Fallthrough!'); break;
    default : break;
}
```
 - All kinds of loops ♾️: 
```dart
take list = [ 'Print', 'these', 'tokens', 'using', 'loopz!' ];

for(take i = 0; i < len(list); i++) 
    System::stdout.write(list[i] + ' ');
    
System::stdout.write('\nThe while loop version: ');

take value;
while((value = list.shift()) != null) 
   System::stdout.write(value + ' ');

System::stdout.write_line('\nDone!');
```
 - Control flow 🎛️:
```dart
take feeling_sad = true;

if(feeling_sad) 
	System::stdout.write_line('Turn up the music if you feel low!');
else System::stdout.write_line('Cheer up the person who is sad beside you. No one should be sad!');

/* The ternary operator. */
take happy = feeling_sad ? 'Was sad but now happy!' : 'Always happy!';
System::stdout.write_line(happy);
```
 - Module support 🧩:
```dart
// say_hello.promit

{{
    return fn() {
        return 'Hello from a module!';
    };
}}
```
```dart
// main.promit

const say_hello = include('say_hello');

say_hello();    // Hello from a module!
```

## Learn 🎓

Current **Promit Test Suite** (Located under ``/test`` relative to current directory) has comprehensive test programs to test against **Promit**. The Test Suite also serve as **_tutorials_** to Promit Programming Language. The programs are lovingly commented in **Teaching** style. Showing code examples with comments make more sense than writing lines of explanation.

## Contribution 🏅
<p>It's a community project. <b>So, feel free to PR 😀.</b> I will try to merge all the pull request related to bug fixes, feature updates and improvements.</p>

**Follow the below rules of contribution:**
 - Leave your fullname, e-mail and your contributed GitHub account username in such manner ``(index) fullname <email> username``. In the next line, please denote your contribution or designation with proper indentation. See ``CONTRIBUTORS`` file for more info.
 - Leave full details of your contribution in the commit changes log and in the pull request description as well.
 - Try leaving comments in the code, as that helps (Like I'm the one to talk 😮‍💨).

<b><i>Happy contribution.</i></b>

## Install 🛠️
Promit comes pre-compiled in GitHub Release. You can grab a release from there.

### Supported and Tested Platforms 💻:
**Linux**: ArchLinux, Debian 10 (Buster) and Debian 11 (Bullseye), Ubuntu 24.04  <b>*BSD's</b> (Not officially supported 😬). 

**Windows:** Windows 7, 10 and 11 (🤢🤢)

**MacOS:** Catelina, BigSur and Ventura

### Download 📥:
Grab the binary releases at: **https://github.com/singul4ri7y/promit/releases**

### How to install 🔧:
 1. Download Promit based on your platform. For example, if you are a Windows user, it is preferable to go for ``promit-<version>-windows-64bit.zip`` for 64 Bit Windows or ``promit-<version>-windows-32bit.zip`` file for 32 Bit Windows (Isn't 32 bit obsolete?).
 2. Extract the archive ``.tar.gz`` for Linux users and ``.zip`` for Windows users.
 3. Under the ``bin/`` folder the the Promit Interpreter binary resides. Move it to any location which is in the **system path** variable.
     - For Windows users, it is recommended to create a folder named ``MyBIN`` on the disk your Windows is installed (Probably the ``C:\`` drive) on and add the folder location to system path variable (How to add folder location to system path variable? Google 🙂).
     - For Linux users, you probably already know what to do. It would be very much preferable to move the binary to ``/usr/local/bin`` where every user can use it. Or create a arbitrary directory in your home to store the binary and add it to system path (Make changes to ``$PATH`` adding your folder location. To make it permanent do it in the  ``/etc/environment`` file).
     - For MacOS/Darwin users, move the downloaded binary to ``/usr/local/bin``.
 4. Now run any Promit program in the terminal or command prompt using ``promit MyProgram.promit``.

<b><i>Good job 😊.</i></b>

## Why (to the author) 🧐
<img src= "logos/logo.jpg" type= "image/jpg" align= "right" width= "350" />

<p align= "justify"><b>Why the heck would I use this language? Or prefer it over my current used language?</b><br>
 The answer is you don't have a particular reason! It's a hobby project. It's neither as fast as JS due to being just a bytecode interpreter nor it has a huge modular library like Python. But hey, everything has to start from somewhere, right? If you find the project interesting or like how the language works, it's specification and implementation, use the language as much as you can and support the project. Who knows, we may reach the level of JavaScript or Python one day 🙂.</p>

<p align= "justify"><b>Why did I bother to do this project at all?</b><br>
 Well, the idea is very simple really. I wanted to make a Programming Language.<br> One thing always fascinated me that, how programming languages function under the hood? How much stuff is getting abstracted from us? You use programming languages to make softwares, right? But some software is directly involved with the programming language (Compilers, Interpreters). How do they work? There are tons of questions unanswered unless I get my hand dirty. So, I got my hands dirty :)</p>

<p align="justify"><b>Why name "Promit"?</b><br>
I started my programming journey by first learning JavaScript. As I got deeper inside the computer science stuff, I reinvented my interests in Low-Level things such as Kernel Development, Memory Managements, Operating Systems, Compiler design, etc. In the meantime, I got into high school and met a very fascinating person, who is one of my dearest friends named <b>Meraj Hossain Promit</b>. He was a real inspiration 💡, a bright man with a bright future, who taught me how to <b>push</b>. We went through a tons of stuff together. I remember one day when I was a first year, I promised him that I would make a full-fledged programming language before I turn 20. He became super happy 😃. When started creating one, I thought why not name after the guy who was always super supportive 😉.</p>

## Build 🏗️
### Prerequisites ✅:
 - ``gcc`` (Tested with GCC 8, 11, 12, 13 and 14; any version greater than 6 will do)
 - ``make`` to run the ``Makefile``.
 - ``git`` to clone the repo (Optional).
 
### Installation 🔨:

**Arch Linux/Manjaro/Arch Based 💥:**
```arch
pacman --sync git base-devel
```
**Debian/Ubuntu/Debian Based 🔥:**
```debian
apt install git build-essential
```
**Windows 🤮:**

The only way is to switch to Linux 😁.

Nah, just kidding.

Install ``MinGW-w64``, ``Cygwin`` or ``TDM GCC`` which has ``make`` baked within it.

Install git from : <a href= "https://git-scm.com/downloads">https://git-scm.com/downloads</a>.

### The easy stuff 💼:
Go to any folder where you want your build files to reside, clone the repo and hit ``make`` in the **terminal**.
```
git clone https://github.com/singul4ri7y/promit
cd promit/promit
make build_release_x64
cd ../bin/Release-x64/
./promit
```
It will start Promit's REPL (Run-Evaluate-Print-Loop) mode. Enjoy the binary.

### Configurations ⚙️:
- ``make`` or ``make build_debug_64bit`` builds the project in Debug configuration 64-bit architecture.
- ``make build_debug_32bit`` builds the project in Debug configuration and 32-bit architecture.
- ``make build_release_64bit`` builds the project in Release configuration and 64-bit architecture.
- ``make build_release_32bit`` builds the project in Release configuration and 32-bit architecture.

<i><b>Peace 😇.</b></i>
