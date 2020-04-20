/*
 * Support for Cryptographic Engine in FPGA card using PCIe interface
 * that can be found on the following platform: Armada. 
 *
 * Author: Duy H.Ho <duyhungho.work@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */
#include <asm/uaccess.h>
#include <asm/smp.h>
#include <crypto/skcipher.h>
#include <crypto/akcipher.h>
#include <crypto/acompress.h>
#include <crypto/rng.h>
#include <crypto/drbg.h>
#include <crypto/kpp.h>
#include <crypto/internal/simd.h>
#include <crypto/aead.h>
#include <crypto/hash.h>
#include <linux/crypto.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/fips.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/scatterlist.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/zlib.h>
#include <linux/once.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "testcrypto.h"

struct tcrypt_result {
    struct completion completion;
    int err;
};
/* tie all data structures together */
struct skcipher_def {
    struct scatterlist sg;
    struct crypto_skcipher *tfm;
    struct skcipher_request *req;
    struct crypto_wait wait;
};
/* Callback function */
static void test_skcipher_cb(struct crypto_async_request *req, int error)
{
    struct tcrypt_result *result = req->data;
    printk(KERN_INFO "Module testcrypto: STARTING test_skcipher_cb\n");
    if (error == -EINPROGRESS)
        return;
    result->err = error;
    complete(&result->completion);
    printk(KERN_INFO "Module testcrypto:Encryption finished successfully\n");
}
/* Perform cipher operation */
static unsigned int test_skcipher_encdec(struct skcipher_def *sk,
                     int enc)
{
    int rc;
    printk(KERN_INFO "Module testcrypto: STARTING test_skcipher_encdec\n");
    if (enc)
        rc = crypto_wait_req(crypto_skcipher_encrypt(sk->req), &sk->wait);
    else
        rc = crypto_wait_req(crypto_skcipher_decrypt(sk->req), &sk->wait);

    if (rc)
            printk(KERN_INFO "skcipher encrypt returned with result %d\n", rc);

    printk(KERN_INFO "Module testcrypto: skcipher encrypt returned with result %d\n", rc);
    return rc;
}

/* Initialize and trigger cipher operation */
static int test_skcipher(void)
{
    struct skcipher_def sk;
    struct crypto_skcipher *skcipher = NULL;
    struct skcipher_request *req = NULL;
    char *scratchpad = NULL;
    char *ivdata = NULL;
    unsigned char key[16];
    int ret = -EFAULT;
    /*
    * Allocate a tfm (a transformation object) and set the key.
    *
    * In real-world use, a tfm and key are typically used for many
    * encryption/decryption operations.  But in this example, we'll just do a
    * single encryption operation with it (which is not very efficient).
     */
    printk(KERN_INFO "Module testcrypto: starting test_skcipher1\n");
    skcipher = crypto_alloc_skcipher("cbc(aes)", 0, 0);
    if (IS_ERR(skcipher)) {
        pr_info("could not allocate skcipher handle\n");
        return PTR_ERR(skcipher);
    }
    /* Allocate a request object */
    req = skcipher_request_alloc(skcipher, GFP_KERNEL);
    if (!req) {
        pr_info("could not allocate skcipher request\n");
        ret = -ENOMEM;
        goto out;
    }

    skcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
                      test_skcipher_cb,
                      &sk.wait);

    /* AES 128 with random key */
    get_random_bytes(&key, 16);
    if (crypto_skcipher_setkey(skcipher, key, 16)) {
        pr_info("key could not be set\n");
        ret = -EAGAIN;
        goto out;
    }

    /* IV will be random */
    ivdata = kmalloc(16, GFP_KERNEL);
    if (!ivdata) {
        pr_info("could not allocate ivdata\n");
        goto out;
    }
    get_random_bytes(ivdata, 16);

    /* Input data will be random */
    scratchpad = kmalloc(16, GFP_KERNEL);
    if (!scratchpad) {
        pr_info("could not allocate scratchpad\n");
        goto out;
    }
    get_random_bytes(scratchpad, 16);

    sk.tfm = skcipher;
    sk.req = req;
    /*
     * Encrypt the data in-place.
     *
     * For simplicity, in this example we wait for the request to complete
     * before proceeding, even if the underlying implementation is asynchronous.
     *
     * To decrypt instead of encrypt, just change crypto_skcipher_encrypt() to
     * crypto_skcipher_decrypt().
     */

    /* We encrypt one block */
    sg_init_one(&sk.sg, scratchpad, 16);
    skcipher_request_set_crypt(req, &sk.sg, &sk.sg, 16, ivdata);
    crypto_init_wait(&sk.wait);

    /* encrypt data */
    ret = test_skcipher_encdec(&sk, 1);
    //printk(KERN_INFO "starting test_skcipher2(ret): %d\n", ret);
    if (ret){
        pr_err("Error encrypting data: %d\n", ret);
        goto out;
    }
    // pr_info("Encryption triggered successfully\n");
    printk(KERN_INFO "Encryption triggered successfully\n");
out:
    if (skcipher)
        crypto_free_skcipher(skcipher);
    if (req)
        skcipher_request_free(req);
    if (ivdata)
        kfree(ivdata);
    if (scratchpad)
        kfree(scratchpad);
    return ret;
}


static int __init test_init(void)
{
    printk(KERN_INFO "Module testcrypto: info: init test\n");
    test_skcipher();
    return 0;
}

static void __exit test_exit(void)
{
    printk(KERN_INFO "Module testcrypto: info: exit test\n");
}

module_init(test_init);
module_exit(test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Duy H.Ho");
MODULE_DESCRIPTION("A prototype Linux module for TESTING crypto in FPGA-PCIE card");
MODULE_VERSION("0.01");