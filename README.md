# printftester_mandatory
Quick mandatory-only printftester for the ft_printf project

<img width="927" height="819" alt="image" src="https://github.com/user-attachments/assets/8abb2a78-74bc-423c-8d56-3b9cd0293a1a" />


## Features

- **Super-fast:** Only tests what you need to pass the mandatory part.
- 💡 **Hints (optional):** Run with `--hints` to get spoilers for each failed case.
- **Minimal: DOES NOT CHECK** for leaks, no bonus. **PLEASE** check for memory issues before submitting.


---

## Quick Usage

Download file
```
curl -o test.c https://raw.githubusercontent.com/nmannage/printftester_mandatory/refs/heads/main/test.c
```

```
make && cc test.c -L. -lftprintf -o test && ./test
```

<img width="927" height="819" alt="image" src="https://github.com/user-attachments/assets/afaf9401-1236-4eea-b0c1-bcb51646ef04" />


```sh
make
cc test.c -L. -lftprintf -o test
./test
./test --hints   # (get debugging tips and spoilers)
```

