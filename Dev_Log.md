# CPU OpenGL Path Tracer Dev Log

## 3/12/2025 22:47
ChatGPT has done pretty much everything up to this point. I came up with some of the logic and was familiar with some of the techniques, but ChatGPT did most of the implementation. I left it off last night after implementing the noise generation and averaging, and must now implement a more efficient randomo generator, and clean up the code.  
With the old random_device/distrib random generation, 1920x1080 is getting about 40FPS. 

## 3/12/2025 22:57
The new random generator is a pseudo-random number geenrator (PRNG) based on an xorshift algorithm. The seed is a "random" number. The lines with "seed ^= seed << x;" then change the seed by shifting left/right (indicated by direction of arrows) by x bits, then XORs with itself. The function then takes the lowest 8 bits of the updated seed, casts it to uint8_t, and therefore returns a random number between 0 and 255. 
With this new bitwise PRNG, a resolution of 1920x1080 is getting about 85FPS. 