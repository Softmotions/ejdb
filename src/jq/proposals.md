
Sample:

```json
  {
     "title": "Hello!",
     "author" : {
       "givenName" : "John",
       "familyName" : "Doe",
       "age": 31,
       "family": {
         "mother": {
           "givenName": "Anna",
           "familyName" : "Doe",
           "age": 55
         }
       }
     },
     "tags":[ "example", "sample" ],
     "content": "This will be unchanged"
   }
```

Filters:

```
/**/[familyName = "Doe"]

/**/[familyName = :?] # Placeholders

/**/[[* = "familyName"] = "Doe"]

/author/familyName

/"author"/"familyName"

/**/familyName

/**/"familyName"

/**/[familyName re "^D.*"]

/**/[familyName like "D*"] and /**/family/mother/[age > 30]

/author/**/mother/[age > 50]

/author/**/mother/[age > 50 and age < 100]

/**/[tags = ["example", "sample"]]

/tags/[* = "sample"]

/tags/[1 = "sample"]

/tags/[* in ["sample", "foo"]]

/tags/[* in ["sample", "foo"] and [* like "ta*"]]

/author/*/mother/[familyName > 10]
```

Projections:

```
/**/[familyName = "Doe" ]  | all

/**/[familyName = "Doe"] | /**/tags + /author/{givenName,familyName}

/**/[familyName = "Doe"] | all - /**/author/{givenName,familyName}

/**/[familyName = "Doe"] and /**/tags/[* = "sample"] | /**/tags + /author/{givenName,familyName}
```

Labels and patching:

Find document then apply JSON patch to it

```
@fname/**/[familyName = "Doe"] | apply [{"op":"replace", path="@fname", value="Hopkins" }]
```

Find document then apply JSON merge patch and projection

```
@fname/**/[familyName = "Doe"]
| apply {"@fname": "Hopkins"}
| /**/tags + /author/{givenName,familyName}
```
