// Shared by the test scripts: a step runner and a few helpers. `t` is the script object.
.pragma library

function find(root, test) {
    const queue = [root]
    while (queue.length > 0) {
        const item = queue.shift()
        if (item !== root && test(item))
            return item
        const children = item.children
        for (let i = 0; children && i < children.length; ++i)
            queue.push(children[i])
    }
    return null
}

function findAll(root, test) {
    const found = []
    const queue = [root]
    while (queue.length > 0) {
        const item = queue.shift()
        if (item !== root && test(item))
            found.push(item)
        const children = item.children
        for (let i = 0; children && i < children.length; ++i)
            queue.push(children[i])
    }
    return found
}
