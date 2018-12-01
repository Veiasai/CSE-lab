# Ansible

使用方法如下：

  1. 下载hadoop-2.8.5.tar.gz，放在roles/hadoop/files目录下。
  2. 腾讯云ssh密钥对功能，新建一对公私钥，并且绑定到四台目标机器。（完全是在腾讯云控制台操作的）
  3. 将私钥放置在根目录（lab4-deploy）下，**权限改为600**。
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
        - 填充了Test_pub，并在config中配置好私钥，运行部署脚本（第8步）后，可以在command中ssh app或者ssh name等，能连接上就算成功。
        - **测试脚本是远程执行的，在lab目录下，写一个文件app_public_ip，这里面的ip应该是app机器的ip，但配置了ssh config之后，写app的host名字就可以。这意味着从自己的电脑能够连接到app机器，测试脚本会把测试与编译后的执行文件，拷贝到app机器上，然后执行，那么app机器也需要能够免密码登陆name和data节点，所以对应Test_pub的密钥，在app节点再放置一份。**
  7. (optional)
      - 有一个问题是，一般公钥认证都是开启的，所以第一次ssh连接，会出现一个ssh交互，yes or no，这让脚本无法运行，有两个办法。（我已经写了该配置在目录下，ansible会优先读取，所以这一条可以不做）
        - 手动先连上去一次。
        - 配置文件/etc/ansible/ansible.cfg的[defaults]中打开注释
            ``` yml
            # uncomment this to disable SSH key host checking
            host_key_checking = False
            ```
  8. 在根目录（lab4-deploy）下运行。
        ``` shell
        ansible-playbook -i hosts site.yml
        ```

  9. 这时应该能够通过part0与part1测试了。
