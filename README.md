# __CPP20-QOI__
A simple **C++20** implementation of the **qoi** format

# Reading
```c++
fileInputStream is("input.qoi");
uint8_t* pixels = qoi::decode(is, &width, &height, &channels, &colorspace);
is.close();
```

# Writing
```c++
fileOutputStream os("output.qoi");
qoi::encode(os, pixels, width, height, channels);
os.flush();
os.close();

```
