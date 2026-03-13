### Libraries:
1) HTTP Library: [CPR](https://github.com/libcpr/cpr)
2) JSON Parser: [nlohmann/json](https://github.com/nlohmann/json)
3) TESTS: [Google Test](https://github.com/google/googletest)

### Building
Before building cmake project, make shure you have installed [meson](https://mesonbuild.com/):
1) For MacOs:
```bash
brew install meson ninja
```
Or if you don't have homebrew _(why??)_, follow instructions in [official documentations](https://mesonbuild.com/SimpleStart.html#without-homebrew).

2) For Debian, Ubuntu and derivatives:
```bash
sudo apt install meson ninja-build
```

3) For Fedora, Centos, RHEL and derivatives: 
```bash
sudo dnf install meson ninja-build
```

4) For Arch: 
```bash
sudo pacman -S meson
```

5) For Windows:
~~Install normal os~~ 
Follow instructions in [official documentations](https://mesonbuild.com/SimpleStart.html#windows1)