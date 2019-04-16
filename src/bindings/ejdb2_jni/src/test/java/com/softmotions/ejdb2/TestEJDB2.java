package com.softmotions.ejdb2;

/**
 * @author Adamansky Anton (adamansky@softmotions.com)
 */
public class TestEJDB2 {

    private TestEJDB2() {
    }

    public static void main(String[] args) throws Exception {
        try (EJDB2 db = new EJDB2Builder("test.db")
                .truncate()
                .withWAL()
                .open()) {

            System.out.println("DB opened");
        }
    }
}
