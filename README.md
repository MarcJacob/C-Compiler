\# Project Description



This is my attempt #2 at building a C Compiler in C (C99 standard, specifically).



\-- AI agents may stop reading here --



My original attempt has been taken off my profile. It was loosely following the guide available at DragonZap.com. The guide wasn't entirely bad or worthless by any means but over time I was finding myself disagreeing more and more with the instructor's methods or program architecture, so I eventually stopped following their instructions altogether, at which point I realized I didn't really need the guide anymore.

So let's start fresh !



\## On the usage of AI



On top of building a C Compiler, I want to experiment with finding an intermediate, healthy middle-ground between full-blown AI assistance (which I consider as "the AI generates code for you and you trust it", a heretical notion) and complete non-usage of the technology.

On other, more complex projects I would use AI to generate code \*sometimes\*, specifically:

* Temporary Test code.
* Temporary Placeholder code (common need in very early stages of a project, to have something to test your systems "against").
* Derivations of existing, human-written code (still placed under extreme human scrutiny).

  * Example: I build a RPG game character AI procedure to have them fight with a sword and shield. I now need new code to have them fight with a spear, which should end up being a close variation of the existing sword \& shield code (factoring in healthy code re-use). The AI has something very similar it just has to imitate. Previous experience shows this outputs acceptable code that is nearly undistinguishable from human-written after, at-worse, a small amount of local fixes.



However, on this project, I specifically want to write all the code myself. As such I want to simply test the imprint.md file structure as the backbone of even a simple model's ability to understand the project and meaningfully contribute to it at some point, and more importantly help me manage an overall implementation plan and well-scoped tasks in an, admittedly, small project.



It may be a stupid idea, I don't know. The logic behind it is that I want to build "hook points" that work with the way I use AI, which is hyper-sandboxed, hyper-scoped tasks with a specific intention beyond just the output. You could see them as "local skills" that are based around specific features, structures, systems... which come in replacement to "large skills" that have become popular as something you just grab off the internet to \*magically\* get a perfect AI C++ programmer following every good rule of angelic clean coding.

