# CSE LAB

听说lab挺多的，用git组织一下好了...

## lab2 bug

- 在lookup函数中多写了一次release，已修复。

## lab3 bug

- rpc一致性在返回值，而不在引用传参，所以不能用引用传参做返回。

## lab4 deploy

- Ansible写了一个部署脚本，详情见lab4-deploy目录。

## lab4 bug

- inode_manager
  - remove_file，free direct block循环，i赋值错误。
  - 修改了bitmap的数据结构，原本是几个block，现在直接用了std::map。
