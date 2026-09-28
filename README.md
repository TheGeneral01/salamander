## Welcome to Salamander!

## Current Status of Code:
Functional-ish, good enough to work, but not efficient enough to make me happy.

### What is Salamander?
Salamander is a programming language designed to leverage the often ignored and underutilized GPU in while loops, and parallel tasks. By using Vulkan as a back-end, all types of GPUs are supported,
from Nvidia, AMD, integrated graphics, and all other Vulkan-Compatible GPUs. It offers dynamic variable support through a type called dyn, which is broken down into static code during compilation.

### Do I need to change the way I write code?
No. SAL was designed to handle all GPU logic in secret on the back-end. It requires no experience with Vulkan at all, and offers great potential performance benefits depending on the task at hand.

### Was SAL created with the use of AI?
Yes, for now. I have a deadline to meet for college apps, and while I do not entirely like relying on AI, it has proved useful in debugging, writing advanced code, and planning architecture for this first iteration.
I will be re-writing this myself, since the AST architecture could be re-engineered to greatly benefit performance, and modular abilities. While I tried to complete most of this code myself, I have
had to rely on AI for helping me meet this deadline more so than usual. For an official release, I will not be using AI outside of basic printing functions for enums. I try to be open about this
policy, since many advanced concepts I can only learn from YouTube videos anyway, and I have yet to properly be formally educated on low-level architecture. I apologize to those of you whom
want complete code now, but that is not quite yet the reality.

### Can SAL use multiple GPUs?
Soon™

### Why bother?
The reason that Sal was created was primarily out of hatred for everything in Python being pure recommendations. I much preferred C++ and statically typed languages, although at times despised
string logic, and the difficulty and memory space that dynamic lists imposed. I wanted the speeds of C++, and the abilities of Python all in one. Then, I wanted something like Joblib, but
just for the GPU. Ultimately, after contemplation, a new programming language was what I settled on, with native Vulkan compatibility, without any GPU knowledge required from a developer. While
not a priority right now, SAL will soon feature native multi-threading support alongside the GPU to offer even further performance benefits where possible.

### Will I still be able to use Vulkan separately?
Yes! However, this is not implemented yet because the SAL runtime is not quite finished, but one of the first features will be direct exposure to the Vulkan instance, as well as simplified GPU
access.

*"A computer can never be held accountable, therefore a computer must never make a management decision."* - **IBM**
