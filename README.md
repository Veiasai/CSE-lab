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
  - 添加appendblock的时候，考虑到complete才会更新size，所以没有更新size，但这样在多次连续append的时候，定位会错误。我想了想，namenode会lock file，更新size应该不会引起并发错误，然后failure recovery的话，这个lab完全没有体现，也就暂且不管，于是append直接更新了size。

- 调试技巧
  - 由于一些微妙的原因，引入了bug，然后花费了数小时审视代码，我审视的部分代码完全与bug无关，最后是开启了VERBOSE和所有文件中检索log信息，才定位问题。
  - 这告诉我两点
    - 增加信息
    - 相信日志，并定位错误
