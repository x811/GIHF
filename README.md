# GIHF
Hi,
This type-2 hypervisor project was laying around on my hard drive for a while, as I am reclaiming space, I found myself not needed in this project anymore as I have less time and desire to make a banger out of it.

The code is commented *mostly* so it shouldn't be much trouble of understanding it, though somewhere it lacks the explanation.

Initially I've had an idea of making it capable to allow the user to reverse any process, including embedding just in time compiled code by the user mode program to kernel/hypervisor space, but I got tired of this big blob of code and only looked at it as a studying experience since then. For now I've seen better approaches in this area and have other work to do.

The thing you are *probably* interested the most is SLAT. Since the start of the project I saw many different ways to use this epic tech to your advantage and most of them use quite a light math to shadow the memory. Zamn, even when I've started other projects I'd grab a pen and a blank sheet to calculate stuff and understand how it works. iirc it's not done right and fully in this project.
