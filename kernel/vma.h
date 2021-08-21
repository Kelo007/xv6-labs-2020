// maxium number of vmas 
#define NVMA (16)
#define PROT_READ (PTE_V | PTE_R | PTE_U)
#define PROT_WRITE (PTE_V | PTE_W | PTE_U)
#define MAP_SHARED 1
#define MAP_PRIVATE 0
struct VMA {
  uint64 addr;
  int length; // if length is zero, it means that this vma is reuseable
  int prot;
  int flags;
  int offset;
  struct file *f;
};