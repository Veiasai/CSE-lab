# CSE LAB

听说lab挺多的，用git组织一下好了...

## lab2 bug

在lookup函数中多写了一次release，已修复。

## lab3 bug

rpc一致性在返回值，而不在引用传参，所以不能用引用传参做返回。

## lab4 deploy

Ansible写了一个部署脚本，使用方法如下：

  1. 下载hadoop-2.8.5.tar.gz，放在roles/hadoop/files目录下。
  2. 腾讯云ssh密钥对功能，新建一对公私钥，并且绑定到四台目标机器。
  3. 将私钥放置在根目录（lab4-deploy）下。
  4. 修改group_vars/cse文件。
        ``` yml
        ansible_ssh_private_key_file: ./cselab.isa
        ```
        指定为刚才第三步放的密钥文件，或者直接把自己的密钥改名为cselab.isa。
  5. 修改hosts文件，填上自己四台机器的公网私网ip
        ``` yml
        [cse]
        app     ansible_ssh_host=129.211.101.125    private_ip=192.168.0.15
        name    ansible_ssh_host=129.211.101.244    private_ip=192.168.0.7
        data1   ansible_ssh_host=212.129.143.248    private_ip=192.168.0.5
        data2   ansible_ssh_host=129.211.102.206    private_ip=192.168.0.17
        ```
  6. (optional)  
        - 这一步是为了能够运行测试，助教测试脚本是打算从其他机器连到那四台机器的cse用户进行测试，所以我们放上了助教的公钥。我们自己是通过腾讯云下发的密钥连接，这个密钥绑定在ubuntu用户，所以有些问题。
        - 在group_vars/cse文件中，预留了一个Test_pub，我们可以使用ssh-keygen生成自己的公私钥，然后将公钥内容copy到这里，私钥留作连接用。
        - 脚本中会使用hostname去连接，所以还需要配置运行测试脚本的机器上的hosts，但是更好的办法是配置ssh config，这个文件在~/.ssh/config。
        - 我给出了一份样例配置ssh-config。
        - 填充了Test_pub，并在config中配置好私钥，运行部署脚本（第7步）后，可以在command中ssh app或者ssh name等，能连接上就算成功。
        - 然后再运行测试。
        - 不过我听助教说，测试脚本是先跳到app在开始测试的，所以密钥在放到app机器的.ssh目录下（实际上，如果想要四台机器都能够通过part0测试，那么这份密钥应该在四个机器上都放好，这样四台机器就互相都可以连接了），以便它能够连接其他机器。
        - 经过我的踩坑，由于测试脚本使用了telnet，所以/etc/hosts还是需要配置一下的。
        - 所以建议还是app上跑测试吧。
  7. 在根目录（lab4-deploy）下运行。
        ``` shell
        ansible-playbook -i hosts site.yml
        ```
