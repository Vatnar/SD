 [![VLA](https://img.shields.io/badge/VLA-181717?style=for-the-badge&logo=github&logoColor=white)](https://github.com/vatnar/VLA)

> [!FAQ] People coming from [SD](https://sd.vatnar.dev)
> Since SD is for learning, instead of just diving in and using [glm](https://github.com/icaven/glm) (definitely check it out :)) I decided to implement my own linear algebra library, which I call Vatnar Linear Algebra. The project is standalone from the engine, but developed at the same time, and therefore it does not implement features I don't have a use for in the engine yet. The repository for VLA can be found [here](https://github.com/Vatnar/VLA). 

# Quickstart
### Using CMake and git submodules (recommended)

Clone the repository into where you want to put it, I recommend in a vendor folder or similar
```bashe
git clone https://github.com/Vatnar/VLA
```
In your main `CMakeLists.txt`:
```cmake
add_subdirectory(vendor/VLA)

target_link_libraries(your_target PUBLIC
	VLA
)
```
You should now be able to include for example
```cpp
#include "VLA/Matrix.hpp"
#include "VLA/Vector.hpp"
```
And use the VLA library.

### Using a distributed release
Currently VLA is only available as source code, but in the future prebuilt binaries will be released for major platforms.